//////////////////////////////////////////////////////////////////////////////
//// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//// PARTICULAR PURPOSE.
////
//// Copyright (c) Microsoft Corporation. All rights reserved
//////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------
// File: BitonicSort.cpp
// 
// Implements Bitonic sort in C++ AMP
// Supports only int, unsigned, long and unsigned long
//----------------------------------------------------------------------------

#include <assert.h>
#include <amp.h>
#include <iostream>

#pragma warning ( disable : 4267)
using namespace concurrency;

#define BITONIC_BLOCK_SIZE          1024
// Should be a square matrix
#define NUM_ELEMENTS                (BITONIC_BLOCK_SIZE * BITONIC_BLOCK_SIZE) 
#define MATRIX_WIDTH                BITONIC_BLOCK_SIZE
#define MATRIX_HEIGHT               BITONIC_BLOCK_SIZE
// Should be divisible by MATRIX_WIDTH and MATRIX_HEIGHT
// else parallel_for_each will crash
#define TRANSPOSE_BLOCK_SIZE        16

// Calls the provided work function and returns the number of milliseconds  
// that it takes to call that function. 
template <class Function>
__int64 time_call(Function&& f)
{
	__int64 begin = GetTickCount();
	f();
	return GetTickCount() - begin;
}

void ComputeMatrixMult(const array<float, 2> &mA, const array<float, 2> &mB,
	array<float, 2> &mC)
 {
	parallel_for_each(mC.extent, [&](index<2> idx) restrict(amp) {
		float result = 0.0f;
		for (int i = 0; i < mA.extent[1]; ++i)
		{
			index<2> idxA(idx[0], i);
			index<2> idxB(i, idx[1]);
			result += mA[idxA] * mB[idxB];
		}
		mC[idx] = result;
	});
}

void WarmUp()
{
	extent<2> eA(1, 1), eB(1, 1), eC(1, 1);
	array<float, 2> mA(eA), mB(eB), mC(eC);
	ComputeMatrixMult(mA, mB, mC);
}

//----------------------------------------------------------------------------
// Kernel implements partial sorting on accelerator, BITONIC_BLOCK_SIZE at a time
//----------------------------------------------------------------------------
template <typename _type>
void bitonic_sort_kernel(array<_type, 1>& data, unsigned ulevel, unsigned ulevelmask, tiled_index<BITONIC_BLOCK_SIZE> tidx) restrict (amp)
{
    tile_static _type sh_data[BITONIC_BLOCK_SIZE];

    int local_idx = tidx.local[0];
    int global_idx = tidx.global[0];

    // Cooperatively load data - each thread will load data from global memory
    // into tile_static
    sh_data[local_idx] = data[global_idx];

    // Wait till all threads have loaded their portion of data
    tidx.barrier.wait();
    
    // Sort data in tile_static memory
    for (unsigned j = ulevel >> 1 ; j > 0 ; j >>= 1)
    {
        _type result = ((sh_data[local_idx & ~j] <= sh_data[local_idx | j]) == (bool)(ulevelmask & global_idx)) ? sh_data[local_idx ^ j] : sh_data[local_idx];
        tidx.barrier.wait();
        sh_data[local_idx] = result;
        tidx.barrier.wait();
    }
    
    // Store shared data
    data[global_idx] = sh_data[local_idx];
}

//----------------------------------------------------------------------------
// Kernel implements 2D matrix transpose
//----------------------------------------------------------------------------
template <typename _type>
void transpose_kernel(array<_type, 1>& data_in, array<_type, 1>& data_out, unsigned width, unsigned height, tiled_index<TRANSPOSE_BLOCK_SIZE, TRANSPOSE_BLOCK_SIZE> tidx) restrict (amp)
{
	tile_static _type transpose_shared_data[TRANSPOSE_BLOCK_SIZE][TRANSPOSE_BLOCK_SIZE];
	extent<2> e_mat_dim(width, height);

    array_view<_type, 2> transpose_matrix_a = data_in.view_as(e_mat_dim);
	transpose_shared_data[tidx.local[0]][tidx.local[1]] = transpose_matrix_a[tidx.global[0]][tidx.global[1]];

    tidx.barrier.wait();

	array_view<_type, 2> transpose_matrix_b = data_out.view_as(e_mat_dim);
	transpose_matrix_b[tidx.global[1]][tidx.global[0]] = transpose_shared_data[tidx.local[0]][tidx.local[1]];
}

//----------------------------------------------------------------------------
// Generate random data
//----------------------------------------------------------------------------
template <typename _type>
void filldata(std::vector<_type>& data)
{
    for (unsigned i = 0; i < data.size(); i++)
        data[i] = (_type)(rand());
}

//----------------------------------------------------------------------------
// CPU helper driving accelerator sorting
//----------------------------------------------------------------------------
template <typename _type>
void bitonic_sort_amp(std::vector<_type>& data_in, std::vector<_type>& data_out)
{
	// Verify assumptions
	assert(NUM_ELEMENTS/MATRIX_WIDTH == MATRIX_WIDTH);
	assert(((MATRIX_WIDTH%TRANSPOSE_BLOCK_SIZE) == 0) && ((MATRIX_HEIGHT%TRANSPOSE_BLOCK_SIZE) == 0));
	assert(data_out.size() == NUM_ELEMENTS);

    array<_type, 1> temp(data_out.size());
    array<_type, 1> data(data_out.size(), data_in.begin());

    // Sort the data
    // First sort the rows for the levels <= to the block size
    extent<1> compute_domain(NUM_ELEMENTS);

    for (unsigned level = 2; level <= BITONIC_BLOCK_SIZE ; level = level * 2 )
    {
        parallel_for_each(compute_domain.tile<BITONIC_BLOCK_SIZE>(),
			[=, &data] (tiled_index<BITONIC_BLOCK_SIZE> tidx) restrict(amp)
        {
            bitonic_sort_kernel<_type>(data, level, level, tidx);
        });
    }

    unsigned ulevel;
    unsigned ulevelMask;
    unsigned width;
    unsigned height;

    // Then sort the rows and columns for the levels > than the block size
    // Transpose. Sort the Columns. Transpose. Sort the Rows.
    for( unsigned level = (BITONIC_BLOCK_SIZE * 2) ; level <= NUM_ELEMENTS ; level = level * 2 )
    {
        ulevel = (level / BITONIC_BLOCK_SIZE);
        ulevelMask = (level & ~NUM_ELEMENTS) / BITONIC_BLOCK_SIZE;
        width = MATRIX_WIDTH;
        height = MATRIX_HEIGHT;

        // Transpose the data from buffer 1 into buffer 2
        extent<2> cdomain_transpose(MATRIX_WIDTH, MATRIX_HEIGHT);
        parallel_for_each (cdomain_transpose.tile<TRANSPOSE_BLOCK_SIZE, TRANSPOSE_BLOCK_SIZE>(), 
            [=, &data, &temp] (tiled_index<TRANSPOSE_BLOCK_SIZE, TRANSPOSE_BLOCK_SIZE> tidx)  restrict(amp)
            {
                transpose_kernel<_type>(data, temp, width, height, tidx);
            });

        // Sort the transposed column data
        extent<1> cdomain_num_elements(NUM_ELEMENTS);
        parallel_for_each(cdomain_num_elements.tile<BITONIC_BLOCK_SIZE>(), [=, &temp] (tiled_index<BITONIC_BLOCK_SIZE> tidx) restrict(amp)
        {
            bitonic_sort_kernel<_type>(temp, ulevel, ulevelMask, tidx);
        });

        ulevel = BITONIC_BLOCK_SIZE;
        ulevelMask = level;
        width = MATRIX_HEIGHT;
        height = MATRIX_WIDTH;

        // Transpose the data from buffer 2 back into buffer 1
        parallel_for_each (cdomain_transpose.tile<TRANSPOSE_BLOCK_SIZE, TRANSPOSE_BLOCK_SIZE>(), 
            [=, &data, &temp] (tiled_index<TRANSPOSE_BLOCK_SIZE, TRANSPOSE_BLOCK_SIZE> tidx)   restrict(amp)
            {
                transpose_kernel<_type>(temp, data, width, height, tidx);
            });

        // Sort the row data
        parallel_for_each(cdomain_num_elements.tile<BITONIC_BLOCK_SIZE>(), [=, &data] (tiled_index<BITONIC_BLOCK_SIZE> tidx) restrict(amp)
        {
            bitonic_sort_kernel<_type>(data, ulevel, ulevelMask, tidx);
        });
    }

	copy(data, data_out.begin());
}

template <typename _type>
bool verify(std::vector<_type>& data)
{
    _type max_so_far = data[0];
    for (unsigned i = 1; i < data.size(); i++)
    {
        if (data[i] < max_so_far)
        {
            printf ("Fail. Index %d\n", i);
            return false;
        }
        max_so_far = data[i];
    }
    return true;
}

int main()
{
	accelerator default_device;
	std::wcout << L"Using device : " << default_device.get_description() << std::endl;
	if (default_device == accelerator(accelerator::direct3d_ref))
		std::cout << "WARNING!! Running on very slow emulator! Only use this accelerator for debugging." << std::endl;

    unsigned length = NUM_ELEMENTS;
	WarmUp();
    {
        std::vector<int> datain(length);
        std::vector<int> dataout(length);
        printf ("Filling with %d int type ...\n", length);
        filldata<int>(datain);

        printf ("Offloading sort to accelerator\n");
		__int64 elapsed = time_call([&] { bitonic_sort_amp<int>(datain, dataout);});
		std::cout << "cost time: " << elapsed << std::endl;

        printf ("Verify data on CPU : ");
        if (verify<int>(dataout))
            printf ("Correct");
        else
            printf ("Incorrect");
        printf ("\n\n");
    }

    return 0;
}