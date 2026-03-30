// waffle-fuse: Amiga OFS / FFS FUSE filesystem driver  (read + write)
//
// Supported formats:
//  - OFS  (DOS\x00) and FFS (DOS\x01)
//  - INTL variants (DOS\x02 / DOS\x03)
//  - DD (880 KB) and HD (1.76 MB) disks
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

// Block types and secondary types
static const uint32_t T_SHORT   = 2;
static const uint32_t T_DATA    = 8;
static const uint32_t T_LIST    = 16;
static const int32_t  ST_ROOT   = 1;
static const int32_t  ST_DIR    = 2;
static const int32_t  ST_FILE   = -3;
static const int32_t  ST_LNKFILE= -4;
static const uint32_t HT_SIZE   = 72;   // hash-table / data-ptr table entries per block

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
    b[o]=(uint8_t)(v>>24); b[o+1]=(uint8_t)(v>>16); b[o+2]=(uint8_t)(v>>8); b[o+3]=(uint8_t)v;
}

static inline void puts32(uint8_t* b, size_t o, int32_t v)
{
	put32(b,o,(uint32_t)v);
}

// Block field accessors
static inline int32_t blk_type   (const uint8_t* b) { return blong(b,0); }
static inline int32_t blk_secType(const uint8_t* b) { return blong(b,508); }
static inline int32_t blk_nextH  (const uint8_t* b) { return blong(b,496); }
static inline int32_t blk_ext    (const uint8_t* b) { return blong(b,504); }
static inline int32_t blk_size   (const uint8_t* b) { return blong(b,324); }
static inline int32_t blk_highSeq(const uint8_t* b) { return blong(b,8); }
static inline int32_t blk_ht     (const uint8_t* b, int i) { return blong(b, 24+(size_t)i*4); }
static inline int32_t blk_dataPtr(const uint8_t* b, int i) { return blong(b, 24+(size_t)i*4); }

static std::string blk_name(const uint8_t* b)
{
    uint8_t len = b[432];
    if (len>30) len=30;
    return std::string(reinterpret_cast<const char*>(b+433), len);
}

static time_t blk_mtime(const uint8_t* b)
{
    int32_t days=blong(b,420), mins=blong(b,424), ticks=blong(b,428);
    return 252460800LL + (time_t)days*86400 + (time_t)mins*60 + ticks/50;
}

// Checksum: sum of all 128 big-endian longs = 0; csoff field adjusted to make it so.
// Bitmap blocks use csoff=0; all other header/data blocks use csoff=20.
static void blk_fix(uint8_t* b, size_t csoff=20)
{
    uint32_t s=0;

    for(size_t i=0;i<512;i+=4)
		if( i!=csoff) s+=ublong(b,i);

    put32(b, csoff, (uint32_t)(-(int32_t)s));
}

// Amiga epoch: 1-Jan-1978 = Unix + 252460800 s
static void unix_to_amiga(time_t t, uint32_t& days, uint32_t& mins, uint32_t& ticks)
{
    if (t<252460800) t=252460800;
    time_t a=t-252460800;
    days=(uint32_t)(a/86400);
    uint32_t r=(uint32_t)(a%86400); mins=r/60; ticks=(r%60)*50;
}

static void blk_set_mtime(uint8_t* b, time_t t=0)
{
    if (!t) t=::time(nullptr);
    uint32_t days,mins,ticks; unix_to_amiga(t,days,mins,ticks);
    put32(b,420,days);
    put32(b,424,mins);
    put32(b,428,ticks);
}

static void blk_set_name(uint8_t* b, const std::string& name)
{
    size_t len=std::min(name.size(),(size_t)30);
    b[432]=(uint8_t)len;
    memcpy(b+433, name.c_str(), len);
    if (len<30) memset(b+433+len, 0, 30-len);
}

// AmigaOS hash (intermediate values masked to 11 bits to match INTL behaviour)
static uint32_t affsHash(const char* name, size_t len, uint32_t tableSize)
{
    uint32_t h=(uint32_t)len;
    for(size_t i=0;i<len;i++)
		h=(h*13 + (uint32_t)toupper((unsigned char)name[i]))&0x7FFu;
    return h % tableSize;
}

// ============================================================================
struct AffsFs {
    IDiskImage* disk      = nullptr;
    uint32_t    numBlocks = 0;
    uint32_t    rootBlock = 0;
    bool        isFFS     = false;

    mutable std::map<uint32_t, std::vector<uint8_t>> cache;

    // Bitmap (lazy-loaded)
    mutable std::vector<uint8_t> bitmapData;
    mutable uint32_t             bitmapBlkNum = 0;
    mutable bool                 bitmapLoaded = false;
    bool                         bitmapDirty  = false;

    bool init(IDiskImage* d)
    {
        disk=d; numBlocks=d->totalSectors(); rootBlock=numBlocks/2;
        uint8_t bb[512]={};
        if(!d->readSector(0,bb)) return false;
        if(bb[0]!='D'||bb[1]!='O'||bb[2]!='S') return false;
        isFFS=(bb[3]&1)!=0;
        uint8_t rb[512]={};
        if(!readBlock(rootBlock,rb)) return false;
        return blk_type(rb)==(int32_t)T_SHORT && blk_secType(rb)==ST_ROOT;
    }

    // Block I/O
    bool readBlock(uint32_t n, uint8_t out[512]) const
    {
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
        cache[n]=std::vector<uint8_t>(b,b+512);
        return disk->writeSector(n,b);
    }
    bool writeFfsDataBlock(uint32_t n, const uint8_t b[512])
    {
        cache[n]=std::vector<uint8_t>(b,b+512);
        return disk->writeSector(n,b);
    }

    // Bitmap helpers
    bool loadBitmap() const
    {
        if(bitmapLoaded) return true;
        uint8_t rb[512]={};
        if(!readBlock(rootBlock,rb)) return false;
        bitmapBlkNum=ublong(rb,316);  // bm_pages[0] at root block offset 316
        if(!bitmapBlkNum||bitmapBlkNum>=numBlocks) return false;
        bitmapData.resize(512,0);
        uint8_t raw[512]={};
        if(!readBlock(bitmapBlkNum,raw)) return false;
        memcpy(bitmapData.data(),raw,512);
        bitmapLoaded=true;
        return true;
    }
    bool saveBitmap()
    {
        if(!bitmapDirty) return true;
        blk_fix(bitmapData.data(),0);
        cache[bitmapBlkNum]=std::vector<uint8_t>(bitmapData.begin(),bitmapData.end());
        if(!disk->writeSector(bitmapBlkNum,bitmapData.data())) return false;
        bitmapDirty=false;
        return true;
    }
    bool isBlkFree(uint32_t n) const
    {
        if(!bitmapLoaded||n>=numBlocks) return false;
        uint32_t li=n/32, bi=n%32;
        return (ublong(bitmapData.data(), 4+li*4)>>bi)&1u;
    }
    void bitmapMark(uint32_t n, bool free)
    {
        uint32_t li=n/32, bi=n%32;
        uint32_t v=ublong(bitmapData.data(), 4+li*4);
        if(free) v|=(1u<<bi); else v&=~(1u<<bi);
        put32(bitmapData.data(), 4+li*4, v);
        bitmapDirty=true;
    }
    int32_t allocBlock()
    {
        if(!loadBitmap()) return -1;
        for(uint32_t n=2;n<numBlocks;n++)
        {
            if(n==rootBlock||n==bitmapBlkNum) continue;
            if(isBlkFree(n))
            {
				bitmapMark(n,false);
				return (int32_t)n;
			}
        }
        return -1;
    }
    bool freeBlk(uint32_t n)
    {
        if(!loadBitmap()) return false;
        bitmapMark(n,true);
        cache.erase(n);
        return true;
    }

    // Directory listing
    struct DirEntry {
        std::string name; uint32_t block; bool isDir,isSoftLink;
        int32_t size; time_t mtime;
    };
    std::vector<DirEntry> listDir(uint32_t dirBlock) const
    {
        std::vector<DirEntry> r;
        uint8_t db[512]={};
        if(!readBlock(dirBlock,db)) return r;
        for(uint32_t i=0;i<HT_SIZE;i++)
        {
            int32_t blk=blk_ht(db,i);
            std::set<uint32_t> seen;
            while(blk>0&&(uint32_t)blk<numBlocks)
            {
                if(!seen.insert((uint32_t)blk).second) break;  // cycle detected
                uint8_t eb[512]={};
                if(!readBlock((uint32_t)blk,eb)) break;
                if(blk_type(eb)!=(int32_t)T_SHORT) break;
                int32_t st=blk_secType(eb);
                if(st!=ST_FILE&&st!=ST_DIR&&st!=ST_LNKFILE) break;
                DirEntry de;
                de.name=blk_name(eb);
                de.block=(uint32_t)blk;
                de.isDir=(st==ST_DIR);
                de.isSoftLink=(st==ST_LNKFILE);
                de.size=(st==ST_FILE)?blk_size(eb):0;
                de.mtime=blk_mtime(eb);
                r.push_back(std::move(de));
                blk=blk_nextH(eb);
            }
        }
        return r;
    }

    // Path resolution
    struct LookupResult {
		bool found=false;
		uint32_t block=0;
		int32_t secType=0;
		int32_t size=0;
		time_t mtime=0;
	};
    static std::vector<std::string> splitPath(const char* p)
    {
        std::vector<std::string> parts; std::string cur;
        for(;*p;++p)
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
    LookupResult lookup(const char* path) const
    {
        LookupResult res;
        auto parts=splitPath(path);
        if(parts.empty())
        {
			res.found=true;
			res.block=rootBlock;
			res.secType=ST_ROOT;
			return res;
		}
        uint32_t cur=rootBlock;
        for(size_t pi=0;pi<parts.size();pi++)
        {
            const std::string& name=parts[pi];
            uint8_t cb[512]={};
            if(!readBlock(cur,cb)) return res;
            uint32_t h=affsHash(name.c_str(),name.size(),HT_SIZE);
            int32_t blk=blk_ht(cb,h);
            bool found=false;
            std::set<uint32_t> seen;
            while(blk>0&&(uint32_t)blk<numBlocks)
            {
                if(!seen.insert((uint32_t)blk).second) break;  // cycle detected
                uint8_t eb[512]={};
                if(!readBlock((uint32_t)blk,eb)) break;
                if(blk_type(eb)!=(int32_t)T_SHORT) break;
                if(strcasecmp(blk_name(eb).c_str(),name.c_str())==0)
                {
                    if(pi==parts.size()-1)
                    {
                        res.found=true;
                        res.block=(uint32_t)blk;
                        res.secType=blk_secType(eb);
                        res.size=blk_size(eb);
                        res.mtime=blk_mtime(eb);
                        return res;
                    }
                    int32_t st=blk_secType(eb);
                    if(st!=ST_DIR && st!=(int32_t)ST_ROOT)
						return res;
                    cur=(uint32_t)blk;
                    found=true;
                    break;
                }
                blk=blk_nextH(eb);
            }
            if(!found) return res;
        }
        return res;
    }

    // File data block list (sequential order; handles extension blocks)
    std::vector<uint32_t> fileDataBlocks(uint32_t fileBlock) const
    {
        std::vector<uint32_t> blocks;
        uint32_t hdr=fileBlock;
        std::set<uint32_t> seenHdr;
        for(;;)
        {
            if(!seenHdr.insert(hdr).second) break;  // cycle detected
            uint8_t fb[512]={};
            if(!readBlock(hdr,fb)) break;
            int32_t hs=blk_highSeq(fb); if(hs<0) hs=0;
            if((uint32_t)hs > HT_SIZE) hs=(int32_t)HT_SIZE; // clamp: corrupt high_seq causes OOM
            // AmigaOS stores data block pointers from the END of the table:
            // index HT_SIZE-1 = first data block, HT_SIZE-2 = second, etc.
            for(int i=(int)HT_SIZE-1; i>=(int)(HT_SIZE-(uint32_t)hs); i--)
            {
                int32_t dp=blk_dataPtr(fb,i);
                if(dp>0 && (uint32_t)dp<numBlocks) blocks.push_back((uint32_t)dp);
            }
            int32_t ext=blk_ext(fb);
            if(ext<=0) break;
            hdr=(uint32_t)ext;
        }
        return blocks;
    }

    // Read file content
    int readFile(uint32_t fileBlock, int32_t fileSize, char* buf, size_t size, off_t offset) const
    {
        if(offset>=fileSize) return 0;
        auto db=fileDataBlocks(fileBlock);
        const uint32_t bds=isFFS?512u:488u, doff=isFFS?0u:24u;
        size_t toRead=std::min(size,(size_t)(fileSize-offset)); size_t done=0;
        for(size_t bi=0;bi<db.size()&&done<toRead;bi++)
        {
            off_t bs=(off_t)(bi*bds), be_=bs+bds;
            if(be_<=offset) continue;
            if(bs>=(off_t)(offset+toRead)) break;
            uint8_t raw[512]={}; if(!readBlock(db[bi],raw)) break;
            const uint8_t* data=raw+doff;
            off_t from=std::max(offset-bs,(off_t)0);
            size_t srcOff=(size_t)(bs+from-offset);  // position in output buf
            size_t avail=(size_t)(bds-from);
            size_t take=std::min(avail,toRead-srcOff);
            memcpy(buf+srcOff,data+from,take); done+=take;
        }
        return (int)done;
    }
    std::vector<uint8_t> readAllFileData(uint32_t fhb, int32_t fsz) const
    {
        std::vector<uint8_t> buf((size_t)(fsz>0?fsz:0),0);
        if(fsz>0) readFile(fhb,fsz,reinterpret_cast<char*>(buf.data()),(size_t)fsz,0);
        return buf;
    }

    // Free all data + extension blocks of a file (not the file header)
    bool freeFileDataBlks(uint32_t fhb)
    {
        auto dbs=fileDataBlocks(fhb);
        for(uint32_t b:dbs)
			freeBlk(b);
        uint8_t fb[512]={};
        if(!readBlock(fhb,fb)) return true;
        int32_t ext=blk_ext(fb);
        std::set<uint32_t> seenExt;
        while(ext>0&&(uint32_t)ext<numBlocks)
        {
            if(!seenExt.insert((uint32_t)ext).second) break;  // cycle detected
            uint8_t eb[512]={};
            if(!readBlock((uint32_t)ext,eb)) break;
            int32_t next=blk_ext(eb);
            freeBlk((uint32_t)ext);
            ext=next;
        }
        return true;
    }

    // Rewrite all file data (free old blocks, alloc new ones, update header & ext blocks)
    int writeFileData(uint32_t fhb, const uint8_t* data, uint32_t size)
    {
        if(disk->isWriteProtected()) return -EROFS;
        uint8_t fh[512]={};
        if(!readBlock(fhb,fh)) return -EIO;
        freeFileDataBlks(fhb);

        const uint32_t bds=isFFS?512u:488u;
        uint32_t ndb=(size+bds-1)/bds;
        if(!size) ndb=0;
        const uint32_t PTRS=HT_SIZE;  // 72 pointers per block

        std::vector<uint32_t> newdb(ndb);
        for(uint32_t i=0;i<ndb;i++)
        {
            int32_t blk=allocBlock();
            if(blk<0)
            {
				for(uint32_t j=0;j<i;j++)
					freeBlk(newdb[j]);
				return -ENOSPC;
			}
            newdb[i]=(uint32_t)blk;
        }

        // Write data blocks
        for(uint32_t i=0;i<ndb;i++)
        {
            uint8_t db[512]={};
            uint32_t off=i*bds, len=std::min(bds,size-off);
            if(isFFS) {
                memcpy(db,data+off,len);
                if(!writeFfsDataBlock(newdb[i],db)) return -EIO;
            } else {
                put32(db,0,T_DATA); put32(db,4,fhb); put32(db,8,i+1); put32(db,12,len);
                put32(db,16,(i+1<ndb)?newdb[i+1]:0u);
                memcpy(db+24,data+off,len);
                if(!writeBlock(newdb[i],db,20)) return -EIO;
            }
        }

        // Build file header + extension blocks
        uint32_t numChunks=(ndb+PTRS-1)/PTRS;
        if(!ndb) numChunks=0;

        // Prepare file header with first chunk
        memset(fh+24,0,PTRS*4);
        uint32_t c0=std::min(ndb,PTRS);
        put32(fh,8,c0);
        put32(fh,16,(ndb>0&&!isFFS)?newdb[0]:0u);
        for(uint32_t i=0;i<c0;i++)
			put32(fh,24+(HT_SIZE-1-i)*4,newdb[i]);
        put32(fh,324,size);
        blk_set_mtime(fh);
        put32(fh,504,0);

        if(numChunks<=1)
        {
            if(!writeBlock(fhb,fh,20)) return -EIO;
        } else {
            // Write extension blocks for chunks 1..N-1
            uint32_t prevBlkNum=fhb;
            uint8_t  prevBlk[512]; memcpy(prevBlk,fh,512);
            for(uint32_t chunk=1;chunk<numChunks;chunk++)
            {
                int32_t extBlk=allocBlock();
                if(extBlk<0) return -ENOSPC;
                uint32_t start=chunk*PTRS, cnt=std::min(ndb-start,PTRS);
                put32(prevBlk,504,(uint32_t)extBlk);
                if(!writeBlock(prevBlkNum,prevBlk,20)) return -EIO;
                uint8_t eb[512]={};
                put32(eb,0,T_LIST); put32(eb,4,fhb); put32(eb,8,cnt);
                puts32(eb,500,(int32_t)fhb); put32(eb,504,0); puts32(eb,508,ST_FILE);
                for(uint32_t i=0;i<cnt;i++)
					put32(eb,24+(HT_SIZE-1-i)*4,newdb[start+i]);
                memcpy(prevBlk,eb,512); prevBlkNum=(uint32_t)extBlk;
            }
            if(!writeBlock(prevBlkNum,prevBlk,20)) return -EIO;
        }
        return saveBitmap()?0:-EIO;
    }

    // Hash-chain management
    bool addToParent(uint32_t parentBlk, uint32_t entryBlk, const std::string& name)
    {
        uint8_t pb[512]={};
        if(!readBlock(parentBlk,pb)) return false;
        uint32_t h=affsHash(name.c_str(),name.size(),HT_SIZE);
        uint8_t eb[512]={};
        if(!readBlock(entryBlk,eb)) return false;
        put32(eb,496,ublong(pb,24+h*4)); // hashChain = current head
        if(!writeBlock(entryBlk,eb,20)) return false;
        put32(pb,24+h*4,entryBlk);
        blk_set_mtime(pb);
        return writeBlock(parentBlk,pb,20);
    }
    bool removeFromParent(uint32_t parentBlk, uint32_t entryBlk, const std::string& name)
    {
        uint8_t pb[512]={};
        if(!readBlock(parentBlk,pb)) return false;
        uint32_t h=affsHash(name.c_str(),name.size(),HT_SIZE);
        uint32_t prev=0, cur=ublong(pb,24+h*4);
        std::set<uint32_t> seen;
        while(cur&&cur<numBlocks)
        {
            if(!seen.insert(cur).second) break;  // cycle detected
            uint8_t eb[512]={};
            if(!readBlock(cur,eb)) return false;
            if(cur==entryBlk)
            {
                uint32_t next=ublong(eb,496);
                if(!prev)
                {
					put32(pb,24+h*4,next);
					blk_set_mtime(pb);
					writeBlock(parentBlk,pb,20); 
				}
                else
                {
                    uint8_t prevb[512]={};
                    readBlock(prev,prevb);
                    put32(prevb,496,next);
                    writeBlock(prev,prevb,20);
                }
                return true;
            }
            prev=cur;
            cur=ublong(eb,496);
        }
        return false;
    }

    // Create new header block (file or directory)
    int32_t doCreateEntry(uint32_t parentBlk, const std::string& name, int32_t sectype)
    {
        if(disk->isWriteProtected() || name.empty() || name.size()>30) return -1;
        int32_t blk=allocBlock();
        if(blk<0) return -1;
        uint8_t b[512]={};
        put32(b,0,T_SHORT); put32(b,4,(uint32_t)blk);
        put32(b,8,0); put32(b,16,0); put32(b,324,0);
        put32(b,496,0); put32(b,500,parentBlk); put32(b,504,0);
        puts32(b,508,sectype);
        blk_set_name(b,name); blk_set_mtime(b);
        if(!writeBlock((uint32_t)blk,b,20))
        {
			freeBlk((uint32_t)blk);
			return -1;
		}
        if(!addToParent(parentBlk,(uint32_t)blk,name))
        {
			freeBlk((uint32_t)blk);
			return -1;
		}
        if(!saveBitmap()) return -1;
        return blk;
    }
};

// FUSE helpers
static AffsFs* getAF() { return static_cast<AffsFs*>(fuse_get_context()->private_data); }

static uint32_t parentBlock(AffsFs* fs, const char* path)
{
    auto parts=AffsFs::splitPath(path);
    if(parts.size()<=1) return fs->rootBlock;
    std::string pp;
    for(size_t i=0;i<parts.size()-1;i++)
		pp+="/"+parts[i];
    auto r=fs->lookup(pp.c_str());
    return r.found ? r.block : fs->rootBlock;
}

// FUSE callbacks
static int affs_getattr(const char* path, struct stat* st, struct fuse_file_info*)
{
    memset(st,0,sizeof(*st));
    AffsFs* fs=getAF();
    auto res=fs->lookup(path);
    if(!res.found) return -ENOENT;
    bool rw=!fs->disk->isWriteProtected();
    if(res.secType==ST_FILE)
    {
		st->st_mode=S_IFREG|(rw?0644:0444); 
		st->st_nlink=1; 
		st->st_size=res.size; 
	}
    else
    {
		st->st_mode=S_IFDIR|(rw?0755:0555); 
		st->st_nlink=2; 
	}
    st->st_mtime=st->st_atime=st->st_ctime=res.mtime;
    return 0;
}

static int affs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                        off_t, struct fuse_file_info*, enum fuse_readdir_flags) 
{
    AffsFs* fs=getAF();
    auto res=fs->lookup(path);
    if(!res.found) return -ENOENT;
    if(res.secType==ST_FILE) return -ENOTDIR;
    filler(buf,".",nullptr,0,FUSE_FILL_DIR_PLUS);
    filler(buf,"..",nullptr,0,FUSE_FILL_DIR_PLUS);
    bool rw=!fs->disk->isWriteProtected();
    for(auto& de:fs->listDir(res.block))
    {
        struct stat s={}; s.st_mtime=de.mtime;
        if(de.isDir)
        {
			s.st_mode=S_IFDIR|(rw?0755:0555);
			s.st_nlink=2;
		}
        else
        {
			s.st_mode=S_IFREG|(rw?0644:0444);
			s.st_nlink=1;
			s.st_size=de.size;
		}
        filler(buf,de.name.c_str(),&s,0,FUSE_FILL_DIR_PLUS);
    }
    return 0;
}

static int affs_open(const char* path, struct fuse_file_info* fi)
{
    AffsFs* fs=getAF();
    auto res=fs->lookup(path);
    if(!res.found) return -ENOENT;
    if(res.secType!=ST_FILE) return -EISDIR;
    if((fi->flags&O_ACCMODE)!=O_RDONLY && fs->disk->isWriteProtected()) return -EROFS;
    return 0;
}

static int affs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info*) 
{
    AffsFs* fs=getAF();
    auto res=fs->lookup(path);
    if(!res.found) return -ENOENT;
    if(res.secType!=ST_FILE) return -EISDIR;
    return fs->readFile(res.block,res.size,buf,size,offset);
}
static int affs_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info*)
{
    AffsFs* fs=getAF();
    if(fs->disk->isWriteProtected()) return -EROFS;
    auto res=fs->lookup(path);
    if(!res.found) return -ENOENT;
    if(res.secType!=ST_FILE) return -EISDIR;
    int32_t newSize=(int32_t)std::max((size_t)res.size,(size_t)(offset+size));
    auto data=fs->readAllFileData(res.block,res.size);
    data.resize((size_t)newSize,0);
    memcpy(data.data()+offset,buf,size);
    int err=fs->writeFileData(res.block,data.data(),(uint32_t)newSize);
    return err?err:(int)size;
}
static int affs_truncate(const char* path, off_t newsz, struct fuse_file_info*)
{
    AffsFs* fs=getAF();
    if(fs->disk->isWriteProtected()) return -EROFS;
    auto res=fs->lookup(path);
    if(!res.found) return -ENOENT;
    if(res.secType!=ST_FILE) return -EISDIR;
    auto data=fs->readAllFileData(res.block,res.size);
    data.resize((size_t)newsz,0);
    return fs->writeFileData(res.block,data.data(),(uint32_t)newsz);
}
static int affs_create(const char* path, mode_t, struct fuse_file_info* fi)
{
    AffsFs* fs=getAF(); 
    if(fs->disk->isWriteProtected()) return -EROFS;
    auto parts=AffsFs::splitPath(path); 
    if(parts.empty()) return -EINVAL;
    uint32_t pb=parentBlock(fs,path);
    auto ex=fs->lookup(path);
    if(ex.found)
    {
		if(ex.secType!=ST_FILE) return -EISDIR; 
		fi->fh=ex.block; 
		return 0; 
	}
    int32_t blk=fs->doCreateEntry(pb,parts.back(),ST_FILE);
    if(blk<0) return -ENOSPC; 
    fi->fh=(uint64_t)blk; 
    return 0;
}
static int affs_mkdir(const char* path, mode_t)
{
    AffsFs* fs=getAF(); 
    if(fs->disk->isWriteProtected()) return -EROFS;
    auto parts=AffsFs::splitPath(path);
    if(parts.empty()) return -EINVAL;
    if(fs->lookup(path).found) return -EEXIST;
    uint32_t pb=parentBlock(fs,path);
    int32_t blk=fs->doCreateEntry(pb,parts.back(),ST_DIR);
    return blk<0?-ENOSPC:0;
}
static int affs_unlink(const char* path)
{
    AffsFs* fs=getAF(); 
    if(fs->disk->isWriteProtected()) return -EROFS;
    auto res=fs->lookup(path); 
    if(!res.found) return -ENOENT;
    if(res.secType!=ST_FILE) return -EISDIR;
    uint8_t fh[512]={}; 
    if(!fs->readBlock(res.block,fh)) return -EIO;
    uint32_t pb=ublong(fh,500);
    fs->freeFileDataBlks(res.block); 
    fs->freeBlk(res.block);
    auto parts=AffsFs::splitPath(path);
    fs->removeFromParent(pb,res.block,parts.back());
    return fs->saveBitmap()?0:-EIO;
}
static int affs_rmdir(const char* path) 
{
    AffsFs* fs=getAF(); 
    if(fs->disk->isWriteProtected()) return -EROFS;
    auto res=fs->lookup(path); 
    if(!res.found) return -ENOENT;
    if(res.secType!=ST_DIR) return -ENOTDIR;
    if(!fs->listDir(res.block).empty()) return -ENOTEMPTY;
    uint8_t db[512]={};
    if(!fs->readBlock(res.block,db)) return -EIO;
    uint32_t pb=ublong(db,500);
    fs->freeBlk(res.block);
    auto parts=AffsFs::splitPath(path);
    fs->removeFromParent(pb,res.block,parts.back());
    return fs->saveBitmap()?0:-EIO;
}
static int affs_rename(const char* from, const char* to, unsigned int flags) 
{
    if(flags) return -EINVAL;
    AffsFs* fs=getAF(); 
    if(fs->disk->isWriteProtected()) return -EROFS;
    auto src=fs->lookup(from); 
    if(!src.found) return -ENOENT;
    auto toParts=AffsFs::splitPath(to); 
    if(toParts.empty()) return -EINVAL;
    std::string dstName=toParts.back();
    uint32_t dstParent=parentBlock(fs,to);
    // Remove existing destination (file only)
    auto dst=fs->lookup(to);
    if(dst.found) 
    {
        if(dst.secType==ST_DIR) return -EISDIR;
        uint8_t dh[512]={}; 
        fs->readBlock(dst.block,dh);
        uint32_t dp=ublong(dh,500);
        fs->freeFileDataBlks(dst.block); 
        fs->freeBlk(dst.block);
        fs->removeFromParent(dp,dst.block,dstName);
    }
    auto fromParts=AffsFs::splitPath(from);
    uint32_t srcParent=parentBlock(fs,from);
    fs->removeFromParent(srcParent,src.block,fromParts.back());
    // Update entry: new name + new parent + clear hashChain
    uint8_t eb[512]={}; 
    if(!fs->readBlock(src.block,eb)) return -EIO;
    blk_set_name(eb,dstName); 
    put32(eb,500,dstParent); 
    put32(eb,496,0);
    if(!fs->writeBlock(src.block,eb,20)) return -EIO;
    if(!fs->addToParent(dstParent,src.block,dstName)) return -EIO;
    return fs->saveBitmap()?0:-EIO;
}
static int affs_utimens(const char* path, const struct timespec tv[2], struct fuse_file_info*) {
    AffsFs* fs=getAF(); if(fs->disk->isWriteProtected()) return -EROFS;
    auto res=fs->lookup(path); if(!res.found) return -ENOENT;
    uint8_t b[512]={}; if(!fs->readBlock(res.block,b)) return -EIO;
    time_t t=(tv&&tv[1].tv_nsec!=UTIME_OMIT)?tv[1].tv_sec:0;
    blk_set_mtime(b,t);
    return fs->writeBlock(res.block,b,20)?0:-EIO;
}
static int affs_chmod(const char*, mode_t, struct fuse_file_info*) { return 0; }
static int affs_chown(const char*, uid_t, gid_t, struct fuse_file_info*) { return 0; }
static int affs_fsync(const char*, int, struct fuse_file_info*) { return 0; }
static int affs_statfs(const char*, struct statvfs* sv) {
    AffsFs* fs = getAF();
    if (!fs->loadBitmap()) return -EIO;
    uint32_t free_blks = 0;
    for (uint32_t n = 2; n < fs->numBlocks; n++) {
        if (n == fs->rootBlock || n == fs->bitmapBlkNum) continue;
        if (fs->isBlkFree(n)) free_blks++;
    }
    memset(sv, 0, sizeof(*sv));
    sv->f_bsize   = 512;
    sv->f_frsize  = 512;
    sv->f_blocks  = fs->numBlocks;
    sv->f_bfree   = free_blks;
    sv->f_bavail  = free_blks;
    sv->f_namemax = 30;
    return 0;
}

static void* affs_fuse_init(struct fuse_conn_info*, struct fuse_config* cfg) {
    cfg->kernel_cache=0; cfg->direct_io=1;
    return fuse_get_context()->private_data;
}

// Public API
bool affs_detect(IDiskImage* disk) {
    uint8_t bb[512]={}; if(!disk->readSector(0,bb)) return false;
    return bb[0]=='D'&&bb[1]=='O'&&bb[2]=='S'&&bb[3]<=5;
}
struct fuse_operations affs_get_operations() {
    struct fuse_operations ops={};
    ops.init=affs_fuse_init;
    ops.getattr=affs_getattr; ops.readdir=affs_readdir;
    ops.open=affs_open;       ops.read=affs_read;
    ops.write=affs_write;     ops.truncate=affs_truncate;
    ops.create=affs_create;   ops.mkdir=affs_mkdir;
    ops.unlink=affs_unlink;   ops.rmdir=affs_rmdir;
    ops.rename=affs_rename;   ops.utimens=affs_utimens;
    ops.chmod=affs_chmod;     ops.chown=affs_chown;
    ops.fsync=affs_fsync;     ops.statfs=affs_statfs;
    return ops;
}
void* affs_create(IDiskImage* disk) {
    AffsFs* fs=new AffsFs(); if(!fs->init(disk)){delete fs;return nullptr;} return fs;
}
void  affs_destroy(void* ctx) { delete static_cast<AffsFs*>(ctx); }
bool  affs_is_ffs(void* ctx)  { return static_cast<AffsFs*>(ctx)->isFFS; }
