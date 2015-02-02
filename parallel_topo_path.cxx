
#include <vector>
#include <algorithm>
#include <iostream>
#include <list>
#include <set>
#include <map>
#include <iterator>
#include <ppl.h>
#include <Windows.h>
#include <mutex>
#include <thread>
#include "d:/WorkSpace/Dxh/RingQueue.h"

using namespace std;
using namespace concurrency;

typedef map<int, int> map_travel_record; // node ptr, sequence no.
typedef map_travel_record::value_type node_with_seq;

namespace std{
	template<>
	struct less<node_with_seq> :public binary_function <node_with_seq, node_with_seq, bool>
	{
		inline bool operator ()(node_with_seq const &left, node_with_seq const &right) const {
			return left.second < right.second;
		}
	};
}

typedef vector<vector<int>> adj_list; // neighbour list table.

template <typename Pred>
int Travel_map(adj_list const &topo,
	map_travel_record rec,
	int node_next,
	Pred &f_term)
{
	if (rec.find(node_next) == rec.end() && !f_term(node_next, rec)) {		
		size_t no = rec.size();
		rec.insert(make_pair(node_next, no));
		// for node_n in adj_list[node_next]. fork Travel_map(topo, rec, node_n, f_term);
		parallel_for(std::size_t(0), topo[node_next].size(),
			[&topo, rec, node_next, &f_term](int n) {
			Travel_map(topo, rec, topo[node_next][n], f_term);
		});
		return 0;
	}
	else {
		return -1;
	}
	return 1;
}

std::mutex g_io_mutex;

const int g_best_count = 5;
int g_best_route_size = 10; // max length of the best_routes.
static RingQueue<vector<int>, g_best_count> g_best_rotues;
// Return true if replaced best; false if discarded.
bool check_best(vector<int> const &route) {
	if (g_best_rotues.size() < g_best_count) {
		g_best_rotues.push_back(route);
		//g_best_route_size = route.size();
		return true;
	}
	int k = 0;
	for (int i = 0; i < g_best_count; ++i){
		if (g_best_rotues[i].size() > g_best_rotues[k].size()) {
			k = i;
		}
	}
	if (route.size() < g_best_rotues[k].size()) {
		g_best_rotues[k] = route;
		g_best_route_size = route.size();
		return true;
	}
	return false;
}

// find the 5 best path in all path.
bool is_done(int dstNode, int curNode, map_travel_record const &route) {
	// Test exclude 
	if (route.size() > g_best_route_size){
		return false;
	}
	if (dstNode == curNode) {
		std::set<node_with_seq> result;
		copy(route.begin(), route.end(), inserter(result, result.begin()));
		// Test
		std::lock_guard<std::mutex> lock(g_io_mutex);
		//cout << "Route[" << 1 + route.size() << "]:";
		vector<int> route_nodes;
		for (auto x : result) {
			route_nodes.push_back(x.first);
		}
		route_nodes.push_back(curNode);
		return check_best(route_nodes);
	}
	return false;
}
// insert edge from topo
bool add_edge(adj_list & topo, int a, int b) {
	if (a == b || a < 0 || b < 0){
		return false;
	}
	if (topo.size() < max(a, b) + 1) {
		topo.resize(max(a, b) + 1);
	}
	if (topo[a].end() != find(topo[a].begin(), topo[a].end(), b) ||
		topo[b].end() != find(topo[b].begin(), topo[b].end(), a))
	{
		return false;
	}
	topo[a].push_back(b);
	topo[b].push_back(a);
	return true;
}
// remove edge from topo
bool remove_edge(adj_list & topo, int a, int b) {
	if (a == b || a < 0 || b < 0){
		return false;
	}
	auto ia = find(topo[a].begin(), topo[a].end(), b);
	auto ib = find(topo[b].begin(), topo[b].end(), a);
	if (topo[a].end() ==  ia||
		topo[b].end() == ib)
	{
		return false;
	}
	topo[a].erase(ia);
	topo[b].erase(ib);
	return true;
}

int main()
{
	adj_list topo;
	const int n = 5;
	for (int r = 0; r < n; ++r) {
		for (int c = 0; c < n; ++c) {
			if (c < n - 1)
				add_edge(topo, n * r + c, n * r + c + 1);
			if (r < n - 1)
				add_edge(topo, n * r + c, n * (r + 1) + c);
		}
	}
	remove_edge(topo, 7, 12);
	remove_edge(topo, 11, 12);
	remove_edge(topo, 13, 12);

	map_travel_record route;
	int iEndNe = 21;
	__int64 begin = GetTickCount();

	Travel_map(topo, route, 0,
		[iEndNe](int curNode, map_travel_record const &route) {
		return is_done(iEndNe, curNode, route);	});
	cout << (GetTickCount() - begin) << "ms\n";
	for (int i = 0; i < g_best_rotues.size(); ++i) {
		cout << "Route[" << 1 + g_best_rotues[i].size() << "]:";
		for (auto x : g_best_rotues[i]) {
			cout << x << ",";
		}
		cout << endl;
	}
	return 0;

}
