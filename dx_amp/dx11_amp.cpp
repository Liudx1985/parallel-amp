#include <amp.h> // C++ AMP 是微软开发的异构并行计算库
#include <iostream>

using namespace concurrency;

#define size_(a) sizeof(a) / sizeof(a[0])

#include <iostream>
#include <iomanip>
#include <Windows.h>
#include <amp.h>
using namespace concurrency;
void default_properties() {
	accelerator default_acc;
	std::wcout << L"Using device : " << default_acc.get_description() << std::endl;
	if (default_acc == accelerator(accelerator::direct3d_ref))
		std::cout << "WARNING!! Running on very slow emulator! \
					 Only use this accelerator for debugging." << std::endl;

	std::vector<accelerator> accs = accelerator::get_all();
	for_each(accs.begin(), accs.end(), [](accelerator acc){
		std::wcout << acc.device_path << "\n";
		std::wcout << acc.dedicated_memory << "\n";
		std::wcout << (acc.supports_cpu_shared_memory ?
			"CPU shared memory: true" : "CPU shared memory: false") << "\n";
		std::wcout << (acc.supports_double_precision ?
			"double precision: true" : "double precision: false") << "\n";
		std::wcout << (acc.supports_limited_double_precision ?
			"limited double precision: true" : "limited double precision: false") << "\n";
	}
	);
}
const int ROWS = 8;
const int COLS = 9;

// tileRow and tileColumn specify the tile that each thread is in.
// globalRow and globalColumn specify the location of the thread in the array_view.
// localRow and localColumn specify the location of the thread relative to the tile.
struct Description {
	int value;
	int tileRow;
	int tileColumn;
	int globalRow;
	int globalColumn;
	int localRow;
	int localColumn;
};

// A helper function for formatting the output.
void SetConsoleColor(int color) {
	int colorValue = (color == 0) ? 4 : 2;
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), colorValue);
}

// A helper function for formatting the output.
void SetConsoleSize(int height, int width) {
	COORD coord; coord.X = width; coord.Y = height;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coord);
	SMALL_RECT* rect = new SMALL_RECT();
	rect->Left = 0; rect->Top = 0; rect->Right = width; rect->Bottom = height;
	SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), true, rect);
}

// This method creates an 8x9 matrix of Description structures. In the call to parallel_for_each, the structure is updated 
// with tile, global, and local indices.
void TilingDescription() {
	// Create 72 (8x9) Description structures.
	std::vector<Description> descs;
	for (int i = 0; i < ROWS * COLS; i++) {
		Description d = { i, 0, 0, 0, 0, 0, 0 };
		descs.push_back(d);
	}

	// Create an array_view from the Description structures.
	extent<2> matrix(ROWS, COLS);
	array_view<Description, 2> descriptions(matrix, descs);

	// Update each Description with the tile, global, and local indices.
	parallel_for_each(descriptions.extent.tile< 2, 3>(),
		[=](tiled_index< 2, 3> t_idx) restrict(amp)
	{
		descriptions[t_idx].globalRow = t_idx.global[0];
		descriptions[t_idx].globalColumn = t_idx.global[1];
		descriptions[t_idx].tileRow = t_idx.tile[0];
		descriptions[t_idx].tileColumn = t_idx.tile[1];
		descriptions[t_idx].localRow = t_idx.local[0];
		descriptions[t_idx].localColumn = t_idx.local[1];
	});

	// Print out the Description structure for each element in the matrix.
	// Tiles are displayed in red and green to distinguish them from each other.
	SetConsoleSize(400, 250);
	for (int row = 0; row < ROWS; row++) {
		for (int column = 0; column < COLS; column++) {
			SetConsoleColor((descriptions(row, column).tileRow + descriptions(row, column).tileColumn) % 2);
			std::cout << "Value: " << std::setw(2) << descriptions(row, column).value << "      ";
		}
		std::cout << "\n";

		for (int column = 0; column < COLS; column++) {
			SetConsoleColor((descriptions(row, column).tileRow + descriptions(row, column).tileColumn) % 2);
			std::cout << "Tile:   " << "(" << descriptions(row, column).tileRow << "," << descriptions(row, column).tileColumn << ")  ";
		}
		std::cout << "\n";

		for (int column = 0; column < COLS; column++) {
			SetConsoleColor((descriptions(row, column).tileRow + descriptions(row, column).tileColumn) % 2);
			std::cout << "Global: " << "(" << descriptions(row, column).globalRow << "," << descriptions(row, column).globalColumn << ")  ";
		}
		std::cout << "\n";

		for (int column = 0; column < COLS; column++) {
			SetConsoleColor((descriptions(row, column).tileRow + descriptions(row, column).tileColumn) % 2);
			std::cout << "Local:  " << "(" << descriptions(row, column).localRow << "," << descriptions(row, column).localColumn << ")  ";
		}
		std::cout << "\n";
		std::cout << "\n";
	}
}

int main() {
	default_properties();
	TilingDescription();
	char wait;
//	std::cin >> wait;
	return 0;
}
// Parallel Add two vector.
int _main() {
	int aCPP[] = { 1, 2, 3, 4, 5 };
	const int size = size_(aCPP);
	int bCPP[] = { 6, 7, 8, 9, 10 };
	int sumCPP[size];
	using size_tt = decltype(sizeof(0));
	// Create C++ AMP objects.
	array_view<const int, 1> a(size, aCPP);
	array_view<const int, 1> b(size, bCPP);
	array_view<int, 1> sum(size, sumCPP);
	sum.discard_data();

	parallel_for_each(
		// Define the compute domain, which is the set of threads that are created.
		sum.extent,
		// Define the code to run on each thread on the accelerator.
		[=](index<1> idx) restrict(amp)
	{
		sum[idx] = a[idx] + b[idx];
	}
	);

	// Print the results. The expected output is "7, 9, 11, 13, 15".
	for (int i = 0; i < size; i++) {
		std::cout << sum[i] << "\n";
	}
	return 0;
}