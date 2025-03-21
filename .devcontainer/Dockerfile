# Use an official Ubuntu as a parent image
FROM ubuntu:20.04

# Install necessary packages
RUN apt-get update &&  DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential \
    g++ \
    cmake \
    git \
    && rm -rf /var/lib/apt/lists/* 