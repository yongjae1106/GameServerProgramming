#include <iostream>
#include <chrono>

int main()
{
	
	for (unsigned int y = 0; y < dimsA.y; ++y)
		for (unsigned int x = 0; x < dimsB.x; x++) {
			for (unsigned int i = 0; i < dimsA.x; ++i)
				h_C[y * dimsB.x + x] += h_A[i + y * dimsA.x] * h_B[i * dimsB.x + x];
		}

}
