// waffle-fuse: Amiga OFS / FFS FUSE filesystem driver  (read + write)
//
// Supported formats:
//  - OFS  (DOS\x00) and FFS (DOS\x01)
//  - INTL variants (DOS\x02 / DOS\x03)
//  - DirCache variants (DOS\x04 / DOS\x05): read-only
//  - DD (880 KB) and HD (1.76 MB) disks
//
// File names are ISO-8859-1 on the Amiga and UTF-8 on Linux; they are
// converted in both directions.  Writes to an open file are buffered and
// committed to the disk when the file is closed (or fsync'ed).
//
// References:
//  - Laurent Clevy: http://lclevy.free.fr/adflib/adf_info.html
//  - adflib by Laurent Clevy (GPL)

#define FUSE_USE_VERSION 31
#include "affs_fs.h"

#include <cstring>
#include <cctype>
#include <ctime>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <set>
#include <cerrno>
#include <unistd.h>

// Block types and secondary types
static const uint32_t T_SHORT    = 2;
static const uint32_t T_DATA     = 8;
static const uint32_t T_LIST     = 16;
static const int32_t  ST_ROOT    = 1;
static const int32_t  ST_DIR     = 2;
static const int32_t  ST_SOFTLINK= 3;
static const int32_t  ST_LINKDIR = 4;
static const int32_t  ST_FILE    = -3;
static const int32_t  ST_LINKFILE= -4;
static const uint32_t HT_SIZE    = 72;   // hash-table / data-ptr table entries per block
static const size_t   MAXNAMELEN = 30;

// Big-endian helpers
static inline int32_t blong(const uint8_t* b, size_t o)
{
    return (int32_t)(((uint32_t)b[o]<<24)|((uint32_t)b[o+1]<<16)|
                     ((uint32_t)b[o+2]<<8)|(uint32_t)b[o+3]);
}

static inline uint32_t ublong(const uint8_t* b, size_t o)
{
    return (uint32_t)blong(b,o);
}

static inline void put32(uint8_t* b, size_t o, uint32_t v)
{
    b[o]=(uint8_t)(v>>24);
    b[o+1]=(uint8_t)(v>>16);
    b[o+2]=(uint8_t)(v>>8);
    b[o+3]=(uint8_t)v;
}

static inline void puts32(uint8_t* b, size_t o, int32_t v)
{
    put32(b,o,(uint32_t)v);
}

// Block field accessors
static inline int32_t blk_type   (const uint8_t* b)
{
    return blong(b,0);
}
static inline int32_t blk_secType(const uint8_t* b)
{
    return blong(b,508);
}
static inline int32_t blk_nextH  (const uint8_t* b)
{
    return blong(b,496);
}
static inline int32_t blk_ext    (const uint8_t* b)
{
    return blong(b,504);
}
static inline int32_t blk_size   (const uint8_t* b)
{
    return blong(b,324);
}
static inline int32_t blk_highSeq(const uint8_t* b)
{
    return blong(b,8);
}
static inline int32_t blk_ht     (const uint8_t* b, int i)
{
    return blong(b, 24+(size_t)i*4);
}
static inline int32_t blk_dataPtr(const uint8_t* b, int i)
{
    return blong(b, 24+(size_t)i*4);
}

// Raw (ISO-8859-1) name of a header block
static std::string blk_name(const uint8_t* b)
{
    uint8_t len = b[432];
    if (len>MAXNAMELEN) len=MAXNAMELEN;
    return std::string(reinterpret_cast<const char*>(b+433), len);
}

// AmigaOS keeps dates in local time (no time zones), Unix in UTC
static long utcOffset(time_t t)
{
    struct tm lt;
    localtime_r(&t, &lt);
    return lt.tm_gmtoff;
}

static time_t blk_mtime(const uint8_t* b)
{
    int32_t days=blong(b,420), mins=blong(b,424), ticks=blong(b,428);
    time_t local = 252460800LL + (time_t)days*86400 + (time_t)mins*60 + ticks/50;
    return local - utcOffset(local);
}

// Checksum: sum of all 128 big-endian longs = 0; csoff field adjusted to make it so.
// Bitmap blocks use csoff=0; all other header/data blocks use csoff=20.
static void blk_fix(uint8_t* b, size_t csoff=20)
{
    uint32_t s=0;

    for(size_t i=0; i<512; i+=4)
        if( i!=csoff) s+=ublong(b,i);

    put32(b, csoff, (uint32_t)(-(int32_t)s));
}

static bool blk_sumOk(const uint8_t* b)
{
    uint32_t s=0;
    for(size_t i=0; i<512; i+=4) s+=ublong(b,i);
    return s==0;
}

// Amiga epoch: 1-Jan-1978 = Unix + 252460800 s (local time)
static void unix_to_amiga(time_t t, uint32_t& days, uint32_t& mins, uint32_t& ticks)
{
    t+=utcOffset(t);
    if (t<252460800) t=252460800;
    time_t a=t-252460800;
    days=(uint32_t)(a/86400);
    uint32_t r=(uint32_t)(a%86400);
    mins=r/60;
    ticks=(r%60)*50;
}

static void blk_set_time(uint8_t* b, size_t off, time_t t=0)
{
    if (!t) t=::time(nullptr);
    uint32_t days,mins,ticks;
    unix_to_amiga(t,days,mins,ticks);
    put32(b,off,days);
    put32(b,off+4,mins);
    put32(b,off+8,ticks);
}

static void blk_set_mtime(uint8_t* b, time_t t=0)
{
    blk_set_time(b,420,t);
}

static void blk_set_name(uint8_t* b, const std::string& name)
{
    size_t len=std::min(name.size(),MAXNAMELEN);
    b[432]=(uint8_t)len;
    memcpy(b+433, name.c_str(), len);
    if (len<MAXNAMELEN) memset(b+433+len, 0, MAXNAMELEN-len);
}

// ── Character set ─────────────────────────────────────────────────────────────

static std::string latin1ToUtf8(const std::string& s)
{
    std::string r;
    for (unsigned char c : s)
    {
        if (c < 0x80) r += (char)c;
        else
        {
            r += (char)(0xC0 | (c >> 6));
            r += (char)(0x80 | (c & 0x3F));
        }
    }
    return r;
}

// Fails (returns false) for characters outside ISO-8859-1.
static bool utf8ToLatin1(const std::string& s, std::string& out)
{
    out.clear();
    for (size_t i = 0; i < s.size(); )
    {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80)
        {
            out += (char)c;
            i++;
            continue;
        }
        if ((c & 0xE0) == 0xC0 && i + 1 < s.size())
        {
            uint32_t cp = ((uint32_t)(c & 0x1F) << 6) | ((unsigned char)s[i+1] & 0x3F);
            if (cp > 0xFF) return false;
            out += (char)cp;
            i += 2;
            continue;
        }
        return false;
    }
    return true;
}

// AmigaDOS upper-casing: ASCII only, plus the Latin-1 letters for INTL volumes
static inline unsigned char amigaUpper(unsigned char c, bool intl)
{
    if (c >= 'a' && c <= 'z') return (unsigned char)(c - 32);
    if (intl && c >= 0xE0 && c <= 0xFE && c != 0xF7) return (unsigned char)(c - 32);
    return c;
}

static bool amigaNameEq(const std::string& a, const std::string& b, bool intl)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (amigaUpper((unsigned char)a[i], intl) != amigaUpper((unsigned char)b[i], intl))
            return false;
    return true;
}

static uint32_t affsHash(const std::string& name, bool intl)
{
    uint32_t h=(uint32_t)name.size();
    for (unsigned char c : name)
        h=(h*13 + amigaUpper(c, intl))&0x7FFu;
    return h % HT_SIZE;
}

// ============================================================================
struct AffsFs
{
    IDiskImage* disk      = nullptr;
    uint32_t    numBlocks = 0;
    uint32_t    rootBlock = 0;
    uint8_t     dosType   = 0;     // last byte of "DOS\x"
    bool        isFFS     = false;
    bool        isIntl    = false;
    bool        fsReadOnly= false; // DirCache or unvalidated bitmap
    std::string volumeName;        // raw Latin-1
    std::string readOnlyReason;    // why fsReadOnly was set, if it was

    mutable std::map<uint32_t, std::vector<uint8_t>> cache;

    // Bitmap (lazy-loaded); bit (n-2) of the map stands for block n
    mutable std::vector<uint8_t> bitmapData;
    mutable uint32_t             bitmapBlkNum = 0;
    mutable bool                 bitmapLoaded = false;
    bool                         bitmapDirty  = false;
    uint32_t                     allocHint    = 0;

    // Open files with buffered contents (fi->fh -> state)
    struct OpenFile
    {
        uint32_t block   = 0;
        bool     loaded  = false;
        bool     dirty   = false;
        bool     deleted = false;
        std::vector<uint8_t> data;
    };
    std::map<uint64_t, OpenFile> files;
    uint64_t nextHandle = 1;

    bool ro() const
    {
        return fsReadOnly || disk->isWriteProtected();
    }

    bool init(IDiskImage* d)
    {
        disk=d;
        numBlocks=d->totalSectors();
        rootBlock=numBlocks/2;
        uint8_t bb[512]= {};
        if(!d->readSector(0,bb)) return false;
        if(bb[0]!='D'||bb[1]!='O'||bb[2]!='S'||bb[3]>7) return false;
        dosType=bb[3];
        isFFS=(dosType&1)!=0;
        isIntl=dosType>=2;
        fsReadOnly=dosType>=4;      // DirCache (4/5) and long names (6/7) are not maintained
        uint8_t rb[512]= {};
        if(!readBlock(rootBlock,rb)) return false;
        if(blk_type(rb)!=(int32_t)T_SHORT || blk_secType(rb)!=ST_ROOT) return false;
        volumeName=blk_name(rb);
        // Only modify volumes AmigaOS itself would accept: a root block with a
        // bad checksum, or a bitmap not marked valid (bm_flag != -1, i.e. the
        // disk needs validating on an Amiga), are mounted read-only.
        if(!blk_sumOk(rb))
        {
            fsReadOnly=true;
            readOnlyReason="root block checksum is wrong";
        }
        else if(blong(rb,312)!=-1)
        {
            fsReadOnly=true;
            readOnlyReason="bitmap not validated";
        }
        else if(!loadBitmap())
        {
            fsReadOnly=true;
            readOnlyReason="bitmap block unreadable or corrupt";
        }
        else if(dosType>=4) readOnlyReason="DirCache/long-name volumes are read-only";
        return true;
    }

    // Block I/O
    bool readBlock(uint32_t n, uint8_t out[512]) const
    {
        if(n>=numBlocks) return false;
        auto it=cache.find(n);
        if(it!=cache.end())
        {
            memcpy(out,it->second.data(),512);
            return true;
        }
        if(!disk->readSector(n,out)) return false;
        cache[n]=std::vector<uint8_t>(out,out+512);
        return true;
    }
    bool writeBlock(uint32_t n, uint8_t b[512], size_t csoff=20)
    {
        blk_fix(b,csoff);
        return writeRaw(n,b);
    }
    bool writeRaw(uint32_t n, const uint8_t b[512])
    {
        if(n>=numBlocks) return false;
        if(!disk->writeSector(n,b))
        {
            cache.erase(n);
            return false;
        }
        cache[n]=std::vector<uint8_t>(b,b+512);
        return true;
    }

    // Bitmap helpers
    bool loadBitmap() const
    {
        if(bitmapLoaded) return true;
        uint8_t rb[512]= {};
        if(!readBlock(rootBlock,rb)) return false;
        bitmapBlkNum=ublong(rb,316);  // bm_pages[0] at root block offset 316
        if(!bitmapBlkNum||bitmapBlkNum>=numBlocks) return false;
        // One bitmap block maps 127*32 = 4064 blocks: enough for DD and HD floppies
        if(numBlocks-2 > 127u*32u) return false;
        uint8_t raw[512]= {};
        if(!readBlock(bitmapBlkNum,raw)) return false;
        if(!blk_sumOk(raw)) return false;
        bitmapData.assign(raw,raw+512);
        bitmapLoaded=true;
        return true;
    }
    bool isBlkFree(uint32_t n) const
    {
        if(!bitmapLoaded||n<2||n>=numBlocks) return false;
        uint32_t bit=n-2, li=bit/32, bi=bit%32;
        return (ublong(bitmapData.data(), 4+li*4)>>bi)&1u;
    }
    void bitmapMark(uint32_t n, bool free)
    {
        if(n<2||n>=numBlocks) return;
        uint32_t bit=n-2, li=bit/32, bi=bit%32;
        uint32_t v=ublong(bitmapData.data(), 4+li*4);
        if(free) v|=(1u<<bi);
        else v&=~(1u<<bi);
        put32(bitmapData.data(), 4+li*4, v);
        bitmapDirty=true;
    }
    // Like the Amiga, allocate close to the root block (shorter seeks)
    int32_t allocBlock()
    {
        if(!loadBitmap()) return -1;
        const uint32_t span=numBlocks-2;
        uint32_t start=allocHint?allocHint:rootBlock;
        for(uint32_t i=0; i<span; i++)
        {
            uint32_t n=2+((start-2+i)%span);
            if(isBlkFree(n))
            {
                bitmapMark(n,false);
                allocHint=n+1<numBlocks?n+1:2;
                return (int32_t)n;
            }
        }
        return -1;
    }
    void freeBlk(uint32_t n)
    {
        if(!loadBitmap()) return;
        bitmapMark(n,true);
        cache.erase(n);
    }
    uint32_t countFree() const
    {
        if(!loadBitmap()) return 0;
        uint32_t c=0;
        for(uint32_t n=2; n<numBlocks; n++) if(isBlkFree(n)) c++;
        return c;
    }

    // Write back the bitmap and stamp the volume's "last altered" date
    bool commitMeta()
    {
        if(bitmapDirty)
        {
            blk_fix(bitmapData.data(),0);
            if(!writeRaw(bitmapBlkNum,bitmapData.data())) return false;
            bitmapDirty=false;
        }
        uint8_t rb[512]= {};
        if(!readBlock(rootBlock,rb)) return false;
        blk_set_time(rb,472);
        return writeBlock(rootBlock,rb,20);
    }

    // Directory listing
    struct DirEntry
    {
        std::string name;
        uint32_t block;
        int32_t secType;
        int32_t size;
        time_t mtime;
    };
    std::vector<DirEntry> listDir(uint32_t dirBlock) const
    {
        std::vector<DirEntry> r;
        uint8_t db[512]= {};
        if(!readBlock(dirBlock,db)) return r;
        std::set<uint32_t> seen;
        for(uint32_t i=0; i<HT_SIZE; i++)
        {
            int32_t blk=blk_ht(db,i);
            while(blk>0&&(uint32_t)blk<numBlocks)
            {
                if(!seen.insert((uint32_t)blk).second) break;  // cycle detected
                uint8_t eb[512]= {};
                if(!readBlock((uint32_t)blk,eb)) break;
                if(blk_type(eb)!=(int32_t)T_SHORT) break;
                DirEntry de;
                de.name=blk_name(eb);
                de.block=(uint32_t)blk;
                de.secType=blk_secType(eb);
                de.size=blk_size(eb);
                de.mtime=blk_mtime(eb);
                r.push_back(std::move(de));
                blk=blk_nextH(eb);
            }
        }
        return r;
    }

    // Path resolution.  Path components are UTF-8; hard links are followed
    // to their real entry.
    struct LookupResult
    {
        bool found=false;
        bool badName=false;     // not representable in Latin-1
        uint32_t block=0;       // header block of the entry (or its real entry for hard links)
        uint32_t entryBlock=0;  // block that sits in the parent's hash chain
        uint32_t parent=0;
        int32_t secType=0;
        int32_t size=0;
        time_t mtime=0;
    };
    static std::vector<std::string> splitPath(const char* p)
    {
        std::vector<std::string> parts;
        std::string cur;
        for(; *p; ++p)
        {
            if(*p=='/')
            {
                if(!cur.empty())
                {
                    parts.push_back(cur);
                    cur.clear();
                }
            }
            else
                cur+=*p;
        }
        if(!cur.empty()) parts.push_back(cur);
        return parts;
    }
    // Find 'name' (Latin-1) in directory 'dirBlk'; 0 if absent
    uint32_t findInDir(uint32_t dirBlk, const std::string& name) const
    {
        uint8_t cb[512]= {};
        if(!readBlock(dirBlk,cb)) return 0;
        int32_t blk=blk_ht(cb,affsHash(name,isIntl));
        std::set<uint32_t> seen;
        while(blk>0&&(uint32_t)blk<numBlocks)
        {
            if(!seen.insert((uint32_t)blk).second) break;  // cycle detected
            uint8_t eb[512]= {};
            if(!readBlock((uint32_t)blk,eb)) break;
            if(blk_type(eb)!=(int32_t)T_SHORT) break;
            if(amigaNameEq(blk_name(eb),name,isIntl)) return (uint32_t)blk;
            blk=blk_nextH(eb);
        }
        return 0;
    }
    LookupResult lookup(const char* path) const
    {
        LookupResult res;
        auto parts=splitPath(path);
        uint32_t cur=rootBlock, parent=0;
        int32_t st=ST_ROOT;
        for(size_t pi=0; pi<parts.size(); pi++)
        {
            if(st!=ST_DIR && st!=ST_ROOT) return res;
            std::string name;
            if(!utf8ToLatin1(parts[pi],name))
            {
                res.badName=true;
                return res;
            }
            uint32_t blk=findInDir(cur,name);
            if(!blk) return res;
            parent=cur;
            cur=blk;
            uint8_t eb[512]= {};
            if(!readBlock(cur,eb)) return res;
            st=blk_secType(eb);
            res.entryBlock=cur;
            if(st==ST_LINKFILE||st==ST_LINKDIR)
            {
                uint32_t real=ublong(eb,468);
                uint8_t rb[512]= {};
                if(!real||!readBlock(real,rb)) return res;
                cur=real;
                st=(st==ST_LINKFILE)?ST_FILE:ST_DIR;
            }
        }
        uint8_t eb[512]= {};
        if(!readBlock(cur,eb)) return res;
        res.found=true;
        res.block=cur;
        if(!res.entryBlock) res.entryBlock=cur;
        res.parent=parent;
        res.secType=(st==ST_ROOT)?ST_DIR:st;
        res.size=blk_size(eb);
        res.mtime=blk_mtime(eb);
        return res;
    }

    // File data block list (sequential order; handles extension blocks)
    // *complete is set to false if an extension block could not be read.
    std::vector<uint32_t> fileDataBlocks(uint32_t fileBlock, std::vector<uint32_t>* extBlocks=nullptr,
                                         bool* complete=nullptr) const
    {
        std::vector<uint32_t> blocks;
        if(complete) *complete=true;
        uint32_t hdr=fileBlock;
        std::set<uint32_t> seenHdr;
        for(;;)
        {
            if(!seenHdr.insert(hdr).second) break;  // cycle detected
            uint8_t fb[512]= {};
            if(!readBlock(hdr,fb))
            {
                if(complete) *complete=false;
                break;
            }
            if(hdr!=fileBlock && extBlocks) extBlocks->push_back(hdr);
            int32_t hs=blk_highSeq(fb);
            if(hs<0) hs=0;
            if((uint32_t)hs > HT_SIZE) hs=(int32_t)HT_SIZE; // clamp: corrupt high_seq causes OOM
            // AmigaOS stores data block pointers from the END of the table:
            // index HT_SIZE-1 = first data block, HT_SIZE-2 = second, etc.
            for(int i=(int)HT_SIZE-1; i>=(int)(HT_SIZE-(uint32_t)hs); i--)
            {
                int32_t dp=blk_dataPtr(fb,i);
                if(dp>0 && (uint32_t)dp<numBlocks) blocks.push_back((uint32_t)dp);
            }
            int32_t ext=blk_ext(fb);
            if(ext<=0||(uint32_t)ext>=numBlocks) break;
            hdr=(uint32_t)ext;
        }
        return blocks;
    }

    // Read file content
    int readFile(uint32_t fileBlock, int32_t fileSize, char* buf, size_t size, off_t offset) const
    {
        if(fileSize<=0||offset>=fileSize) return 0;
        auto db=fileDataBlocks(fileBlock);
        const uint32_t bds=isFFS?512u:488u, doff=isFFS?0u:24u;
        size_t toRead=std::min(size,(size_t)(fileSize-offset));
        size_t done=0;
        // Missing data blocks (unreadable extension block) are an I/O error.  Never
        // return a short read: the kernel would take it as end of file and
        // silently shrink the file.
        if((uint64_t)offset+toRead > (uint64_t)db.size()*bds) return -EIO;
        for(size_t bi=0; bi<db.size()&&done<toRead; bi++)
        {
            off_t bs=(off_t)(bi*bds), be_=bs+bds;
            if(be_<=offset) continue;
            if(bs>=(off_t)(offset+toRead)) break;
            uint8_t raw[512]= {};
            if(!readBlock(db[bi],raw)) return -EIO;
            const uint8_t* data=raw+doff;
            off_t from=std::max(offset-bs,(off_t)0);
            size_t srcOff=(size_t)(bs+from-offset);  // position in output buf
            size_t avail=(size_t)(bds-from);
            size_t take=std::min(avail,toRead-srcOff);
            memcpy(buf+srcOff,data+from,take);
            done+=take;
        }
        return (int)done;
    }
    bool readAllFileData(uint32_t fhb, int32_t fsz, std::vector<uint8_t>& out) const
    {
        out.assign((size_t)(fsz>0?fsz:0),0);
        if(fsz<=0) return true;
        return readFile(fhb,fsz,reinterpret_cast<char*>(out.data()),(size_t)fsz,0)==fsz;
    }

    // Free all data + extension blocks of a file (not the file header)
    void freeFileDataBlks(uint32_t fhb)
    {
        std::vector<uint32_t> exts;
        auto dbs=fileDataBlocks(fhb,&exts);
        for(uint32_t b:dbs) freeBlk(b);
        for(uint32_t b:exts) freeBlk(b);
    }

    // Rewrite all file data (free old blocks, alloc new ones, update header & ext blocks)
    int writeFileData(uint32_t fhb, const uint8_t* data, uint32_t size)
    {
        if(ro()) return -EROFS;
        uint8_t fh[512]= {};
        if(!readBlock(fhb,fh)) return -EIO;

        const uint32_t bds=isFFS?512u:488u;
        const uint32_t PTRS=HT_SIZE;  // 72 pointers per block
        const uint32_t ndb=size?(size+bds-1)/bds:0;
        const uint32_t numChunks=ndb?(ndb+PTRS-1)/PTRS:0;
        const uint32_t needExt=numChunks>1?numChunks-1:0;

        // Check for space before touching anything (old blocks become reusable).
        // Never rewrite a file whose block list cannot be read completely:
        // the unread blocks would stay allocated forever.
        std::vector<uint32_t> oldExt;
        bool complete=true;
        uint32_t oldBlocks=(uint32_t)fileDataBlocks(fhb,&oldExt,&complete).size()+(uint32_t)oldExt.size();
        if(!complete) return -EIO;
        if(ndb+needExt > countFree()+oldBlocks) return -ENOSPC;

        freeFileDataBlks(fhb);

        std::vector<uint32_t> newdb(ndb), ext(needExt);
        for(uint32_t i=0; i<ndb; i++)
        {
            int32_t blk=allocBlock();
            if(blk<0) return -ENOSPC;
            newdb[i]=(uint32_t)blk;
        }
        for(uint32_t i=0; i<needExt; i++)
        {
            int32_t blk=allocBlock();
            if(blk<0) return -ENOSPC;
            ext[i]=(uint32_t)blk;
        }

        // Write data blocks
        for(uint32_t i=0; i<ndb; i++)
        {
            uint8_t db[512]= {};
            uint32_t off=i*bds, len=std::min(bds,size-off);
            if(isFFS)
            {
                memcpy(db,data+off,len);
                if(!writeRaw(newdb[i],db)) return -EIO;
            }
            else
            {
                put32(db,0,T_DATA);
                put32(db,4,fhb);
                put32(db,8,i+1);
                put32(db,12,len);
                put32(db,16,(i+1<ndb)?newdb[i+1]:0u);
                memcpy(db+24,data+off,len);
                if(!writeBlock(newdb[i],db,20)) return -EIO;
            }
        }

        // Extension blocks for chunks 1..N-1
        for(uint32_t chunk=1; chunk<numChunks; chunk++)
        {
            uint32_t start=chunk*PTRS, cnt=std::min(ndb-start,PTRS);
            uint8_t eb[512]= {};
            put32(eb,0,T_LIST);
            put32(eb,4,ext[chunk-1]);
            put32(eb,8,cnt);
            for(uint32_t i=0; i<cnt; i++)
                put32(eb,24+(HT_SIZE-1-i)*4,newdb[start+i]);
            put32(eb,500,fhb);
            put32(eb,504,chunk<needExt?ext[chunk]:0u);
            puts32(eb,508,ST_FILE);
            if(!writeBlock(ext[chunk-1],eb,20)) return -EIO;
        }

        // File header with the first chunk
        memset(fh+24,0,PTRS*4);
        uint32_t c0=std::min(ndb,PTRS);
        put32(fh,8,c0);
        put32(fh,16,ndb?newdb[0]:0u);
        for(uint32_t i=0; i<c0; i++)
            put32(fh,24+(HT_SIZE-1-i)*4,newdb[i]);
        put32(fh,324,size);
        blk_set_mtime(fh);
        put32(fh,504,needExt?ext[0]:0u);
        if(!writeBlock(fhb,fh,20)) return -EIO;
        return commitMeta()?0:-EIO;
    }

    // Hash-chain management
    bool addToParent(uint32_t parentBlk, uint32_t entryBlk, const std::string& name)
    {
        uint8_t pb[512]= {};
        if(!readBlock(parentBlk,pb)) return false;
        uint32_t h=affsHash(name,isIntl);
        uint8_t eb[512]= {};
        if(!readBlock(entryBlk,eb)) return false;
        put32(eb,496,ublong(pb,24+h*4)); // hashChain = current head
        put32(eb,500,parentBlk);
        if(!writeBlock(entryBlk,eb,20)) return false;
        put32(pb,24+h*4,entryBlk);
        blk_set_mtime(pb);
        return writeBlock(parentBlk,pb,20);
    }
    bool removeFromParent(uint32_t parentBlk, uint32_t entryBlk, const std::string& name)
    {
        uint8_t pb[512]= {};
        if(!readBlock(parentBlk,pb)) return false;
        uint32_t h=affsHash(name,isIntl);
        uint32_t prev=0, cur=ublong(pb,24+h*4);
        std::set<uint32_t> seen;
        while(cur&&cur<numBlocks)
        {
            if(!seen.insert(cur).second) break;  // cycle detected
            uint8_t eb[512]= {};
            if(!readBlock(cur,eb)) return false;
            uint32_t next=ublong(eb,496);
            if(cur==entryBlk)
            {
                if(!prev)
                {
                    put32(pb,24+h*4,next);
                    blk_set_mtime(pb);
                    return writeBlock(parentBlk,pb,20);
                }
                uint8_t prevb[512]= {};
                if(!readBlock(prev,prevb)) return false;
                put32(prevb,496,next);
                if(!writeBlock(prev,prevb,20)) return false;
                blk_set_mtime(pb);
                return writeBlock(parentBlk,pb,20);
            }
            prev=cur;
            cur=next;
        }
        return false;
    }

    // Create new header block (file or directory); returns block or -errno
    int32_t doCreateEntry(uint32_t parentBlk, const std::string& name, int32_t sectype)
    {
        if(ro()) return -EROFS;
        if(name.empty()) return -EINVAL;
        if(name.size()>MAXNAMELEN) return -ENAMETOOLONG;
        if(name.find(':')!=std::string::npos) return -EINVAL;
        int32_t blk=allocBlock();
        if(blk<0) return -ENOSPC;
        uint8_t b[512]= {};
        put32(b,0,T_SHORT);
        put32(b,4,(uint32_t)blk);
        put32(b,500,parentBlk);
        puts32(b,508,sectype);
        blk_set_name(b,name);
        blk_set_mtime(b);
        if(!writeBlock((uint32_t)blk,b,20) || !addToParent(parentBlk,(uint32_t)blk,name))
        {
            freeBlk((uint32_t)blk);
            commitMeta();
            return -EIO;
        }
        if(!commitMeta()) return -EIO;
        return blk;
    }

    // ── Open file buffers ─────────────────────────────────────────────────────
    OpenFile* handle(uint64_t fh)
    {
        auto it=files.find(fh);
        return it==files.end()?nullptr:&it->second;
    }
    OpenFile* loadedHandleFor(uint32_t block)
    {
        for(auto& kv:files)
            if(kv.second.block==block && kv.second.loaded && !kv.second.deleted) return &kv.second;
        return nullptr;
    }
    // Load the file into the handle buffer (shared with any other loaded handle)
    int ensureLoaded(OpenFile& of)
    {
        if(of.loaded) return 0;
        if(OpenFile* other=loadedHandleFor(of.block))
        {
            of.data=other->data;
            of.dirty=other->dirty;
            of.loaded=true;
            return 0;
        }
        uint8_t fh[512]= {};
        if(!readBlock(of.block,fh)) return -EIO;
        if(!readAllFileData(of.block,blk_size(fh),of.data)) return -EIO;
        of.loaded=true;
        return 0;
    }
    // Propagate a modified buffer to the other handles of the same file
    void shareBuffer(const OpenFile& src)
    {
        for(auto& kv:files)
        {
            OpenFile& o=kv.second;
            if(&o!=&src && o.block==src.block && !o.deleted)
            {
                o.data=src.data;
                o.loaded=true;
                o.dirty=src.dirty;
            }
        }
    }
    int commit(OpenFile& of)
    {
        if(!of.dirty||of.deleted) return 0;
        int err=writeFileData(of.block,of.data.data(),(uint32_t)of.data.size());
        if(err) return err;
        for(auto& kv:files)
            if(kv.second.block==of.block) kv.second.dirty=false;
        return 0;
    }
};

// FUSE helpers
static AffsFs* getAF()
{
    return static_cast<AffsFs*>(fuse_get_context()->private_data);
}

// Parent directory block for a new entry at 'path', plus its Latin-1 name.
static int parentAndName(AffsFs* fs, const char* path, uint32_t& parent, std::string& name)
{
    auto parts=AffsFs::splitPath(path);
    if(parts.empty()) return -EINVAL;
    if(!utf8ToLatin1(parts.back(),name)) return -EILSEQ;
    if(name.size()>MAXNAMELEN) return -ENAMETOOLONG;
    parent=fs->rootBlock;
    if(parts.size()>1)
    {
        std::string pp;
        for(size_t i=0; i<parts.size()-1; i++) pp+="/"+parts[i];
        auto r=fs->lookup(pp.c_str());
        if(!r.found) return -ENOENT;
        if(r.secType!=ST_DIR) return -ENOTDIR;
        parent=r.block;
    }
    return 0;
}

static void fillStat(AffsFs* fs, struct stat* st, int32_t secType, int32_t size, time_t mtime)
{
    bool rw=!fs->ro();
    if(secType==ST_FILE)
    {
        st->st_mode=S_IFREG|(rw?0644:0444);
        st->st_nlink=1;
        st->st_size=size;
    }
    else if(secType==ST_SOFTLINK)
    {
        st->st_mode=S_IFLNK|0777;
        st->st_nlink=1;
    }
    else
    {
        st->st_mode=S_IFDIR|(rw?0755:0555);
        st->st_nlink=2;
    }
    st->st_blksize=512;
    st->st_blocks=(st->st_size+511)/512;
    st->st_uid=getuid();
    st->st_gid=getgid();
    st->st_mtime=st->st_atime=st->st_ctime=mtime;
}

// FUSE callbacks
static int affs_getattr(const char* path, struct stat* st, struct fuse_file_info*)
{
    memset(st,0,sizeof(*st));
    AffsFs* fs=getAF();
    auto res=fs->lookup(path);
    if(!res.found) return -ENOENT;
    int32_t size=res.size;
    if(res.secType==ST_FILE)
        if(auto* of=fs->loadedHandleFor(res.block)) size=(int32_t)of->data.size();
    fillStat(fs,st,res.secType,size,res.mtime);
    return 0;
}

static int affs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                        off_t, struct fuse_file_info*, enum fuse_readdir_flags)
{
    AffsFs* fs=getAF();
    auto res=fs->lookup(path);
    if(!res.found) return -ENOENT;
    if(res.secType!=ST_DIR) return -ENOTDIR;
    filler(buf,".",nullptr,0,(fuse_fill_dir_flags)0);
    filler(buf,"..",nullptr,0,(fuse_fill_dir_flags)0);
    for(auto& de:fs->listDir(res.block))
    {
        struct stat s= {};
        int32_t st=de.secType;
        if(st==ST_LINKFILE) st=ST_FILE;
        if(st==ST_LINKDIR)  st=ST_DIR;
        if(st!=ST_FILE&&st!=ST_DIR&&st!=ST_SOFTLINK) continue;
        fillStat(fs,&s,st,de.size,de.mtime);
        filler(buf,latin1ToUtf8(de.name).c_str(),&s,0,(fuse_fill_dir_flags)0);
    }
    return 0;
}

static int affs_readlink(const char* path, char* buf, size_t size)
{
    AffsFs* fs=getAF();
    auto res=fs->lookup(path);
    if(!res.found) return -ENOENT;
    if(res.secType!=ST_SOFTLINK) return -EINVAL;
    uint8_t b[512]= {};
    if(!fs->readBlock(res.block,b)) return -EIO;
    std::string target(reinterpret_cast<const char*>(b+24), strnlen(reinterpret_cast<const char*>(b+24),512-224));
    target=latin1ToUtf8(target);
    if(size==0) return -EINVAL;
    snprintf(buf,size,"%s",target.c_str());
    return 0;
}

static int affs_open(const char* path, struct fuse_file_info* fi)
{
    AffsFs* fs=getAF();
    auto res=fs->lookup(path);
    if(!res.found) return -ENOENT;
    if(res.secType!=ST_FILE) return -EISDIR;
    bool wr=(fi->flags&O_ACCMODE)!=O_RDONLY;
    if(wr && fs->ro()) return -EROFS;
    if(wr && res.entryBlock!=res.block) return -EPERM;   // writing through hard links is not supported
    uint64_t h=fs->nextHandle++;
    AffsFs::OpenFile of;
    of.block=res.block;
    // libfuse enables atomic O_TRUNC: the truncation arrives here, not via truncate()
    if(wr && (fi->flags&O_TRUNC))
    {
        int err=fs->ensureLoaded(of);
        if(err) return err;
        if(!of.data.empty())
        {
            of.data.clear();
            of.dirty=true;
        }
    }
    auto& slot=fs->files[h];
    slot=std::move(of);
    if(slot.dirty) fs->shareBuffer(slot);
    fi->fh=h;
    return 0;
}

static int affs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
    AffsFs* fs=getAF();
    AffsFs::OpenFile* of=fi?fs->handle(fi->fh):nullptr;
    uint32_t block;
    if(of)
    {
        if(!of->loaded) if(auto* o=fs->loadedHandleFor(of->block)) of=o;
        if(of->loaded)
        {
            if(offset>=(off_t)of->data.size()) return 0;
            size_t n=std::min(size,of->data.size()-(size_t)offset);
            memcpy(buf,of->data.data()+offset,n);
            return (int)n;
        }
        block=of->block;
    }
    else
    {
        auto res=fs->lookup(path);
        if(!res.found) return -ENOENT;
        if(res.secType!=ST_FILE) return -EISDIR;
        block=res.block;
    }
    uint8_t fh[512]= {};
    if(!fs->readBlock(block,fh)) return -EIO;
    return fs->readFile(block,blk_size(fh),buf,size,offset);
}

static int affs_write(const char*, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
    AffsFs* fs=getAF();
    if(fs->ro()) return -EROFS;
    AffsFs::OpenFile* of=fs->handle(fi->fh);
    if(!of) return -EBADF;
    int err=fs->ensureLoaded(*of);
    if(err) return err;
    size_t end=(size_t)offset+size;
    if(end>(size_t)fs->numBlocks*512) return -EFBIG;
    if(of->data.size()<end) of->data.resize(end,0);
    memcpy(of->data.data()+offset,buf,size);
    of->dirty=true;
    fs->shareBuffer(*of);
    return (int)size;
}

static int affs_truncate(const char* path, off_t newsz, struct fuse_file_info* fi)
{
    AffsFs* fs=getAF();
    if(fs->ro()) return -EROFS;
    if(newsz<0) return -EINVAL;
    if((uint64_t)newsz>(uint64_t)fs->numBlocks*512) return -EFBIG;
    AffsFs::OpenFile* of=fi?fs->handle(fi->fh):nullptr;
    uint32_t block;
    if(of) block=of->block;
    else
    {
        auto res=fs->lookup(path);
        if(!res.found) return -ENOENT;
        if(res.secType!=ST_FILE) return -EISDIR;
        if(res.entryBlock!=res.block) return -EPERM;
        block=res.block;
        of=fs->loadedHandleFor(block);
    }
    if(of)
    {
        int err=fs->ensureLoaded(*of);
        if(err) return err;
        of->data.resize((size_t)newsz,0);
        of->dirty=true;
        fs->shareBuffer(*of);
        return 0;
    }
    std::vector<uint8_t> data;
    uint8_t fh[512]= {};
    if(!fs->readBlock(block,fh)) return -EIO;
    if(!fs->readAllFileData(block,blk_size(fh),data)) return -EIO;
    data.resize((size_t)newsz,0);
    return fs->writeFileData(block,data.data(),(uint32_t)newsz);
}

static int affs_flush(const char*, struct fuse_file_info* fi)
{
    AffsFs* fs=getAF();
    AffsFs::OpenFile* of=fs->handle(fi->fh);
    return of?fs->commit(*of):0;
}

static int affs_release(const char*, struct fuse_file_info* fi)
{
    AffsFs* fs=getAF();
    AffsFs::OpenFile* of=fs->handle(fi->fh);
    int err=of?fs->commit(*of):0;
    fs->files.erase(fi->fh);
    return err;
}

static int affs_fsync(const char*, int, struct fuse_file_info* fi)
{
    AffsFs* fs=getAF();
    AffsFs::OpenFile* of=fi?fs->handle(fi->fh):nullptr;
    int err=of?fs->commit(*of):0;
    if(err) return err;
    return fs->disk->flush()?0:-EIO;
}

static int affs_create(const char* path, mode_t, struct fuse_file_info* fi)
{
    AffsFs* fs=getAF();
    if(fs->ro()) return -EROFS;
    uint32_t pb;
    std::string name;
    int err=parentAndName(fs,path,pb,name);
    if(err) return err;
    auto ex=fs->lookup(path);
    if(ex.found) return -EEXIST;
    int32_t blk=fs->doCreateEntry(pb,name,ST_FILE);
    if(blk<0) return blk;
    uint64_t h=fs->nextHandle++;
    AffsFs::OpenFile of;
    of.block=(uint32_t)blk;
    of.loaded=true;
    fs->files[h]=of;
    fi->fh=h;
    return 0;
}

static int affs_mkdir(const char* path, mode_t)
{
    AffsFs* fs=getAF();
    if(fs->ro()) return -EROFS;
    uint32_t pb;
    std::string name;
    int err=parentAndName(fs,path,pb,name);
    if(err) return err;
    if(fs->lookup(path).found) return -EEXIST;
    int32_t blk=fs->doCreateEntry(pb,name,ST_DIR);
    return blk<0?blk:0;
}

static int affs_unlink(const char* path)
{
    AffsFs* fs=getAF();
    if(fs->ro()) return -EROFS;
    auto res=fs->lookup(path);
    if(!res.found) return -ENOENT;
    if(res.secType==ST_DIR) return -EISDIR;
    if(res.entryBlock!=res.block) return -EPERM;   // hard links not supported for writing
    uint8_t fh[512]= {};
    if(!fs->readBlock(res.block,fh)) return -EIO;
    if(ublong(fh,472)) return -EPERM;              // file has hard links pointing to it
    if(res.secType==ST_FILE)
    {
        std::vector<uint32_t> ext;
        bool complete=true;
        fs->fileDataBlocks(res.block,&ext,&complete);
        if(!complete) return -EIO;                 // unreadable block list: would leak blocks
    }
    if(!fs->removeFromParent(res.parent,res.block,blk_name(fh))) return -EIO;
    if(res.secType==ST_FILE) fs->freeFileDataBlks(res.block);
    fs->freeBlk(res.block);
    for(auto& kv:fs->files)
        if(kv.second.block==res.block) kv.second.deleted=true;
    return fs->commitMeta()?0:-EIO;
}

static int affs_rmdir(const char* path)
{
    AffsFs* fs=getAF();
    if(fs->ro()) return -EROFS;
    auto res=fs->lookup(path);
    if(!res.found) return -ENOENT;
    if(res.secType!=ST_DIR) return -ENOTDIR;
    if(res.block==fs->rootBlock) return -EBUSY;
    if(res.entryBlock!=res.block) return -EPERM;
    if(!fs->listDir(res.block).empty()) return -ENOTEMPTY;
    uint8_t db[512]= {};
    if(!fs->readBlock(res.block,db)) return -EIO;
    if(ublong(db,472)) return -EPERM;
    if(!fs->removeFromParent(res.parent,res.block,blk_name(db))) return -EIO;
    fs->freeBlk(res.block);
    return fs->commitMeta()?0:-EIO;
}

static int affs_rename(const char* from, const char* to, unsigned int flags)
{
    if(flags) return -EINVAL;
    AffsFs* fs=getAF();
    if(fs->ro()) return -EROFS;
    auto src=fs->lookup(from);
    if(!src.found) return -ENOENT;
    if(src.block==fs->rootBlock) return -EBUSY;
    if(src.entryBlock!=src.block) return -EPERM;

    uint32_t dstParent;
    std::string dstName;
    int err=parentAndName(fs,to,dstParent,dstName);
    if(err) return err;
    if(dstName.find(':')!=std::string::npos) return -EINVAL;

    // A directory cannot be moved below itself
    if(src.secType==ST_DIR)
    {
        uint32_t p=dstParent;
        std::set<uint32_t> seen;
        while(p && p!=fs->rootBlock && seen.insert(p).second)
        {
            if(p==src.block) return -EINVAL;
            uint8_t pb[512]= {};
            if(!fs->readBlock(p,pb)) return -EIO;
            p=ublong(pb,500);
        }
    }

    auto dst=fs->lookup(to);
    if(dst.found && dst.block==src.block)
    {
        // Same entry: only the case of the name changes (or nothing at all)
    }
    else if(dst.found)
    {
        if(dst.entryBlock!=dst.block) return -EPERM;
        if(src.secType==ST_DIR)
        {
            if(dst.secType!=ST_DIR) return -ENOTDIR;
            int e=affs_rmdir(to);
            if(e) return e;
        }
        else
        {
            if(dst.secType==ST_DIR) return -EISDIR;
            int e=affs_unlink(to);
            if(e) return e;
        }
    }

    uint8_t eb[512]= {};
    if(!fs->readBlock(src.block,eb)) return -EIO;
    if(!fs->removeFromParent(src.parent,src.block,blk_name(eb))) return -EIO;
    if(!fs->readBlock(src.block,eb)) return -EIO;
    blk_set_name(eb,dstName);
    put32(eb,496,0);
    if(!fs->writeBlock(src.block,eb,20)) return -EIO;
    if(!fs->addToParent(dstParent,src.block,dstName)) return -EIO;
    return fs->commitMeta()?0:-EIO;
}

static int affs_utimens(const char* path, const struct timespec tv[2], struct fuse_file_info*)
{
    AffsFs* fs=getAF();
    if(fs->ro()) return -EROFS;
    auto res=fs->lookup(path);
    if(!res.found) return -ENOENT;
    if(tv && tv[1].tv_nsec==UTIME_OMIT) return 0;
    uint8_t b[512]= {};
    if(!fs->readBlock(res.block,b)) return -EIO;
    time_t t=(tv&&tv[1].tv_nsec!=UTIME_NOW)?tv[1].tv_sec:0;
    blk_set_mtime(b,t);
    return fs->writeBlock(res.block,b,20)?0:-EIO;
}
static int affs_chmod(const char*, mode_t, struct fuse_file_info*)
{
    return 0;
}
static int affs_chown(const char*, uid_t, gid_t, struct fuse_file_info*)
{
    return 0;
}
static int affs_statfs(const char*, struct statvfs* sv)
{
    AffsFs* fs = getAF();
    uint32_t free_blks = fs->countFree();
    memset(sv, 0, sizeof(*sv));
    sv->f_bsize   = 512;
    sv->f_frsize  = 512;
    sv->f_blocks  = fs->numBlocks;
    sv->f_bfree   = free_blks;
    sv->f_bavail  = free_blks;
    sv->f_namemax = MAXNAMELEN;
    return 0;
}

static void* affs_fuse_init(struct fuse_conn_info*, struct fuse_config* cfg)
{
    cfg->kernel_cache=0;
    return fuse_get_context()->private_data;
}

// Public API
bool affs_detect(IDiskImage* disk)
{
    uint8_t bb[512]= {};
    if(!disk->readSector(0,bb)) return false;
    return bb[0]=='D'&&bb[1]=='O'&&bb[2]=='S'&&bb[3]<=7;
}
struct fuse_operations affs_get_operations()
{
    struct fuse_operations ops= {};
    ops.init=affs_fuse_init;
    ops.getattr=affs_getattr;
    ops.readdir=affs_readdir;
    ops.readlink=affs_readlink;
    ops.open=affs_open;
    ops.read=affs_read;
    ops.write=affs_write;
    ops.truncate=affs_truncate;
    ops.flush=affs_flush;
    ops.release=affs_release;
    ops.create=affs_create;
    ops.mkdir=affs_mkdir;
    ops.unlink=affs_unlink;
    ops.rmdir=affs_rmdir;
    ops.rename=affs_rename;
    ops.utimens=affs_utimens;
    ops.chmod=affs_chmod;
    ops.chown=affs_chown;
    ops.fsync=affs_fsync;
    ops.statfs=affs_statfs;
    return ops;
}
void* affs_create(IDiskImage* disk)
{
    AffsFs* fs=new AffsFs();
    if(!fs->init(disk))
    {
        delete fs;
        return nullptr;
    }
    return fs;
}
void  affs_destroy(void* ctx)
{
    AffsFs* fs=static_cast<AffsFs*>(ctx);
    for(auto& kv:fs->files) fs->commit(kv.second);
    delete fs;
}
bool  affs_is_ffs(void* ctx)
{
    return static_cast<AffsFs*>(ctx)->isFFS;
}
bool  affs_is_readonly(void* ctx)
{
    return static_cast<AffsFs*>(ctx)->ro();
}
std::string affs_dostype(void* ctx)
{
    return "DOS" + std::to_string(static_cast<AffsFs*>(ctx)->dosType);
}
std::string affs_readonly_reason(void* ctx)
{
    return static_cast<AffsFs*>(ctx)->readOnlyReason;
}
std::string affs_volume_name(void* ctx)
{
    return latin1ToUtf8(static_cast<AffsFs*>(ctx)->volumeName);
}
