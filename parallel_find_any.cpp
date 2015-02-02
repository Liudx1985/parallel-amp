#include <Windows.h>
#include <algorithm>
#include <iostream>
#include <random>
#include <ppl.h>

using namespace concurrency;
using namespace std;

// Calls the provided work function and returns the number of milliseconds  
// that it takes to call that function. 
template <class Function, typename R = result_of<Function()>::type>
inline R time_call(Function&& f)
{
	__int64 begin = GetTickCount();
	R r =  f();
	cout << (GetTickCount() - begin) << "ms\n";
	return r;
}

// Returns the position in the provided array that contains the given value,  
// or -1 if the value is not in the array. 
template<typename _Iter, typename P>
inline _Iter parallel_find_if_any(_Iter _First, _Iter _Last, P &&f)
{
	// The position of the element in the array.  
	// The default value, -1, indicates that the element is not in the array. 
	int position = -1;
	size_t count = std::distance(_First, _Last);
	// Call parallel_for in the context of a cancellation token to search for the element.
	// Call parallel_for in the context of a cancellation token to search for the element.
	cancellation_token_source cts;
	run_with_cancellation_token([count, &f, &_First, &position, &cts]()
	{
		parallel_for(std::size_t(0), count, [&f, &_First, &position, &cts](int n) {
			if (f(*(_First + n)))
			{
				// Set the return value and cancel the remaining tasks.
				position = n;
				cts.cancel();
			}
		});
	}, cts.get_token());

	return position < 0 ? _Last : _First + position;
}

// Determines whether the input value is prime. 
inline bool is_prime(int n)
{
	if (n < 2)
		return false;
	for (int i = 2; i < n; ++i)
	{
		if ((n % i) == 0)
			return false;
	}
	return true;
}
// Determines whether the input value is a Carmichael number. 
inline bool is_carmichael(const int n)
{
	if (n < 2)
		return false;

	int k = n;
	for (int i = 2; i <= k / i; ++i)
	{
		if (k % i == 0)
		{
			if ((k / i) % i == 0)
				return false;
			if ((n - 1) % (i - 1) != 0)
				return false;
			k /= i;
			i = 1;
		}
	}
	return k != n && (n - 1) % (k - 1) == 0;
}
// main entry.
int main()
{
	// For this example, the size must be a power of two. 
	const int size = 0x200000;

	// Create two large arrays and fill them with random values. 
	int* a1 = new int[size];
	int* a2 = new int[size];

	mt19937 gen(42);
	for (int i = 0; i < size; ++i)
	{
		a1[i] = a2[i] = gen();
	}
	
	// Perform the serial version of the sort.
	wcout << "serial time: ";
	auto p1 = time_call([&] {
		return std::find_if(a1, a1 + size, is_carmichael);
	});
	cout << "[" << p1 - a1 << "]" << *p1 << endl;

	// Now perform the parallel version of the find_if_any.
	wcout << "parallel time: ";
	auto p2 = time_call([&] {
		return parallel_find_if_any(a2, a2 + size, is_carmichael);
	});
	cout << "[" << p2 - a2 << "]" << *p2 << endl;	

	delete[] a1;
	delete[] a2;
	return 0;
}