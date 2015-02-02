#include <windows.h>
#include <algorithm>
#include <iostream>
#include <random>
#include <queue>
#include <array>
#include <iterator>
#include <numeric>
#include <list>
#include <ppl.h>
#include <concurrent_vector.h>
#include <ppltasks.h> // for task

using namespace concurrency;
using namespace std;

template <class Function>
inline
__int64 time_call(Function&& f)
{
	__int64 begin = GetTickCount64();
	f();
	return GetTickCount64() - begin;
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
// Sorts the given sequence in the specified order. 
template <class _Iter, class _OutTy, typename _Pred>
inline void parallel_copy_if(_Iter _beg, _Iter _end, _OutTy _to, _Pred &&flt)
{
	typedef typename _Iter::value_type T;

#ifndef _COMBINE
	combinable<vector<T> > cache;
	parallel_for_each(_beg, _end, [&](T i) {
		if (flt(i))
			cache.local().push_back(i);
	});
	cache.combine_each([&](vector<T> &local) {
		std::move(local.begin(), local.end(), _to);
	});
	
#else
	concurrent_vector<T> cache;

	parallel_for_each(_beg, _end, [&](T i) {
		if (flt(i))
			cache.push_back(i);
	});
	std::move(cache.begin(), cache.end(), _to);
#endif // use combine class
}

int main()
{
	array<int, 250000> a;
	vector<int> prime_numbers;

	// Initialize the array such that a[i] == i.
	iota(begin(a), end(a), 100);

	cout << "copy_if used " << time_call([&](){
		copy_if(begin(a), end(a), std::back_inserter(prime_numbers), is_carmichael);
	}) << "ms\n";
	cout << prime_numbers.size();	
	cout << ":[";
	for_each(prime_numbers.begin(), prime_numbers.end(), [](int x){ cout << x << ','; });
	cout << "\n";
	
	prime_numbers.clear();
	cout << "parallel_copy_if used " << time_call([&](){
		parallel_copy_if(begin(a), end(a), std::back_inserter(prime_numbers), is_carmichael);
		parallel_sort(prime_numbers.begin(), prime_numbers.end());
	}) << "ms\n";
	cout << prime_numbers.size();
	cout << ":[";
	for_each(prime_numbers.begin(), prime_numbers.end(), [](int x){ cout << x << ','; });
	cout << "]\n";
}