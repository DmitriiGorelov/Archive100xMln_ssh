
#include "cuda_runtime.h"
#include "device_launch_parameters.h"

#include <thrust/host_vector.h>
#include <thrust/device_vector.h>
#include <cuda.h>

#include <stdio.h>
#include "cudacaller.h"

__global__ void runKernel(const unsigned char* bits, int* bytes, const int sizeBits)
{
    int tid = threadIdx.x;
    int gid = blockDim.x * blockIdx.x;
    int i = gid + tid;

    if (i < sizeBits)
        atomicOr(&bytes[i / 8], ((bits[i] > 0) << (i % 8)));
}

bool cudaCaller::canCuda()
{
    int deviceCount = 0;
    cudaGetDeviceCount(&deviceCount);

    if (deviceCount == 0)
    {
        printf("No CUDA support device found");
        return false;
    }
    return true;
}

bool cudaCaller::doCuda(std::vector<unsigned char>& hostBits, std::vector<unsigned char>& hostBytes, int sizeBits)
{
    thrust::device_vector<unsigned char> deviceBits(hostBits.begin(), hostBits.end());
    thrust::device_vector<int> deviceBytes(hostBytes.size(), 0); // int!

    unsigned char* deviceBitsPtr = thrust::raw_pointer_cast(deviceBits.data());
    int* deviceBytesPtr = thrust::raw_pointer_cast(deviceBytes.data());

    int devNo = 0;
    cudaDeviceProp iProp;
    cudaGetDeviceProperties(&iProp, devNo);

    dim3 blockDim = iProp.maxThreadsPerBlock;
    dim3 gridDim = (sizeBits + blockDim.x - 1) / blockDim.x;

    cudaError_t cudaStatus;
    runKernel << <gridDim, blockDim >> > (deviceBitsPtr, deviceBytesPtr, sizeBits);
    cudaStatus = cudaDeviceSynchronize();
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaDeviceSynchronize returned error code %d after launching runKernel!\n", cudaStatus);
        return false;
    }

    thrust::copy(deviceBytes.begin(), deviceBytes.end(), hostBytes.begin());

    return true;
}

