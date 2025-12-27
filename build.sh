#!/bin/bash

# Define image name
IMAGE_NAME="wafflecopypro-builder"

# Build the Docker image
echo "Building Docker image: $IMAGE_NAME"
docker build -t $IMAGE_NAME -f Dockerfile .

if [ $? -ne 0 ]; then
    echo "Docker image build failed."
    exit 1
fi

# Create a directory for the output AppImage if it doesn't exist
mkdir -p ./dist

# Run the Docker container and extract the AppImage
echo "Running Docker container and extracting AppImage..."
docker run --rm -v "$(pwd)/dist:/app/release" $IMAGE_NAME

if [ $? -ne 0 ]; then
    echo "Docker container run failed."
    exit 1
fi

echo "Build process completed. AppImage should be in the 'dist' directory."
