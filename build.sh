#!/bin/bash

# Define image name
IMAGE_NAME="wafflecopypro-builder"
CONTAINER_NAME="wafflecopypro-build-temp" # Name for the temporary container

# Build the Docker image
echo "Building Docker image: $IMAGE_NAME"
docker build -t $IMAGE_NAME -f Dockerfile .

if [ $? -ne 0 ]; then
    echo "Docker image build failed."
    exit 1
fi

# Create a directory for the output AppImage if it doesn't exist
mkdir -p ./dist

# Run the Docker container to build the AppImage
echo "Running Docker container to build AppImage..."
# We run it, but don't remove it immediately, so we can copy from it
docker run --name $CONTAINER_NAME $IMAGE_NAME

if [ $? -ne 0 ]; then
    echo "Docker container run failed."
    # Clean up the container even if it failed
    docker rm $CONTAINER_NAME
    exit 1
fi

# Copy the AppImage from the container to the host
echo "Copying AppImage from container to host..."
# Assuming the AppImage is named WaffleCopyPRO-Universal-x86_64.AppImage in /app/waffleCopyPRO-Universal/
docker cp $CONTAINER_NAME:/app/waffleCopyPRO-Universal/WaffleCopyPRO-Universal-x86_64.AppImage ./dist/

if [ $? -ne 0 ]; then
    echo "Failed to copy AppImage from container."
    docker rm $CONTAINER_NAME
    exit 1
fi

# Clean up the temporary container
docker rm $CONTAINER_NAME

echo "Build process completed. AppImage should be in the 'dist' directory."