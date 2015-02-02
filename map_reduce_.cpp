// parallel-fibonacci.cpp 
// compile with: /EHsc
#include <windows.h>
#include <ppl.h>
#include <concurrent_vector.h>
#include <ppltasks.h> // for task
#include <array>
#include <vector>
#include <tuple>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <numeric>
#include "inttypes.h" // For Printf Macros(PRId64,etc)
using namespace concurrency;
using namespace std;

// Calls the provided work function and returns the number of milliseconds  
// that it takes to call that function. 
template <class Function>
__int64 time_call(Function&& f)
{
	__int64 begin = GetTickCount64();
	f();
	return GetTickCount64() - begin;
}

void test_task()
{
	array<task<int>, 3> tasks =
	{
		create_task([]() -> int { return 88; }),
		create_task([]() -> int { return 42; }),
		create_task([]() -> int { return 99; })
	};

	auto joinTask = when_all(begin(tasks), end(tasks)).then([](vector<int> results)
	{
		cout << "The sum is "
			<< accumulate(begin(results), end(results), 0)
			<< '.' << endl;
	});

	// Print a message from the joining thread.
	cout << "Hello from the joining thread." << endl;

	// Wait for the tasks to finish.
	joinTask.wait();
}

// Determines whether the input value is prime. 
bool is_prime(int n)
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

void test_map_recude()
{
#ifdef USE_REDUCE // Use parallel_reduce
	// Create an array object that contains 200000 integers. 
	array<int, 200000> a;

	// Initialize the array such that a[i] == i.
	iota(begin(a), end(a), 0);

	int prime_sum;
	__int64 elapsed;

	// Compute the sum of the numbers in the array that are prime.
	elapsed = time_call([&] {
		transform(begin(a), end(a), begin(a), [](int i) {
			return is_prime(i) ? i : 0;
		});
		prime_sum = accumulate(begin(a), end(a), 0);
	});
	cout << prime_sum << endl;
	
	cout << "serial time: " << elapsed
		<< " ms" << endl << endl;

	// Now perform the same task in parallel.
	elapsed = time_call([&] {
		parallel_transform(begin(a), end(a), begin(a), [](int i) {
			return is_prime(i) ? i : 0;
		});
		prime_sum = parallel_reduce(begin(a), end(a), 0);
	});
	cout << prime_sum << endl;
	
	cout << "parallel time: "<< elapsed
		<< " ms" << endl << endl;
#else
	// Create an array object that contains 200000 integers. 
	array<int, 200000> a;

	// Initialize the array such that a[i] == i.
	iota(begin(a), end(a), 0);

	int prime_sum;
	__int64 elapsed;

	// Compute the sum of the numbers in the array that are prime.
	elapsed = time_call([&] {
		prime_sum = accumulate(begin(a), end(a), 0, [&](int acc, int i) {
			return acc + (is_prime(i) ? i : 0);
		});
	});
	wcout << prime_sum << endl;
	wcout << "serial time: " << elapsed << " ms" << endl << endl;

	// Now perform the same task in parallel.
	elapsed = time_call([&] {
		combinable<int> sum;
		parallel_for_each(begin(a), end(a), [&](int i) {
			sum.local() += (is_prime(i) ? i : 0);
		});

		prime_sum = sum.combine(plus<int>());
	});
	wcout << prime_sum << endl;
	wcout << "parallel time: " << elapsed << " ms" << endl << endl;
#endif 

}

int main()
{
	//__int64 i = 0x7fffffffffffffff;
	//std::cout << sizeof(__int64) << '\n' << i << std::endl;
	//printf("%" PRId64 "\t\n", i);
	//printf("%llx\n", i); // PRIx64

	test_map_recude();
	return 0;
}