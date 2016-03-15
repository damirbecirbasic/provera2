#include <cilk/cilk.h>
#include <iostream>
#include <stdlib.h>
#include <algorithm>

#include "BitmapRawConverter.h"
#include "cilktime.h"


#define __ARG_NUM__ 6
#define __NUM_OF_SPAWNS__ 3

using namespace std;

/**
* @brief Serial version of moving average filter.
* @param inBuffer buffer of input image
* @param outBuffer buffer of output image
* @param width image width
* @param height image height
*/
void filter_serial_avg(int* inBuffer, int* outBuffer, int width, int height)
{
	for (int i = 1; i < width - 1; i ++)
	{
		for (int j = 1; j < height - 1; j++)
		{
			// naredne dve petlje realizuju formulu dvostruke sume iz prateceg dokumenta
			for (int m = -1; m <= 1; m++)
			{
				for (int n = -1; n <= 1; n++)
				{
					outBuffer[j * width + i] += inBuffer[(j + n) * width + (i + m)] / 9;
				}
			}
		}
	}
}

/**
* @brief Pomocna funkcija za racunanja avg od dijela kolone
*
* @param inBuffer ulazni bafer
* @param outBuffer izlazni bafer
* @param width sirina slike
* @param start pocetak segmenta kolone koju obradjujemo
* @param kraj segmenta kolone koju obradjujemo
*/
void filter_parallel_avg_spawned(int* inBuffer, int* outBuffer, int width, int start, int end)
{
	for (int i = 1; i < width - 1; i++)
	{
		for (int j = start; j < end ; j++)
		{
			outBuffer[j * width + i] = __sec_reduce_add(inBuffer[(j - 1)*width + i:3] + inBuffer[j*width + i:3] + inBuffer[(j + 1)*width + i:3]) / 9;
		}
	}
}


/**
* @brief Parallel version of moving average filter.
* 
* @param inBuffer buffer of input image
* @param outBuffer buffer of output image
* @param width image width
* @param height image height
*/
void filter_parallel_avg(int* inBuffer, int* outBuffer, int width, int height)
{
	// funkcije dijeli height na __NUM_OF_SPAWNS__ dijelova

	int start = 1;
	int end = height / __NUM_OF_SPAWNS__;

	for (int i = 0; i < __NUM_OF_SPAWNS__; i++)
	{
		// parametri (int* inBuffer, int* outBuffer, int width, int start, int end)
		cilk_spawn filter_parallel_avg_spawned(inBuffer, outBuffer, width, start, end - 1);
		start = end - 1;	
		end += height / __NUM_OF_SPAWNS__ - 1; // -1 zbog obrade zadnjeg el u segmentu
	}

	filter_parallel_avg_spawned(inBuffer, outBuffer, width, start, height - 1); // heigth - 1 zbog obrade zadnjeg reda

	cilk_sync;

}

/**
* @brief racuna median za date indekse
* @param inBuffer ulazni bafer
* @param width sirina slike
* @param xPos red
* @param yPos kolona
*/
int getMedian(int* inBuffer, int width, int xPos, int yPos)
{
	int nizMed[9];
	int ind = 0;

	for (int i = -1; i < 2; i++)
	{
		for (int j = -1; j < 2; j++)
		{
			nizMed[ind++] = inBuffer[(xPos + i) * width + (yPos + j)];
		}
	}

	sort(&nizMed[0], &nizMed[8]);

	return nizMed[4];
}

/**
* @brief Serial version of median filter.
* @param inBuffer buffer of input image
* @param outBuffer buffer of output image
* @param width image width
* @param height image height
*/
void filter_serial_med(int* inBuffer, int* outBuffer, int width, int height)
{
	

	for (int i = 1; i < width - 1; i++)
	{
		for (int j = 1; j < height - 1; j++)
		{
			outBuffer[j * width + i] = getMedian(inBuffer, width, j, i);
		}
	}
}

/**
* @brief paralelno racuna Median za datu poziciju
* @param intBuffer ulazni bafer
* @param width sirina slike
* @param xPos redovi
* @param yPos kolone
*/
int getParallelMedian(int* inBuffer, int width, int xPos, int yPos)
{
	int nizMed[9];
	
	nizMed[0:3] = inBuffer[(xPos - 1) * width + (yPos - 1) : 3];
	nizMed[3:3] = inBuffer[xPos * width + (yPos - 1) : 3];
	nizMed[6:3] = inBuffer[(xPos + 1) * width + (yPos - 1) : 3];

	// 4 puta nadjemo indeks maksimalnog el. i 
	// el. u nizu sa tim indeksom postavimo na jako mali broj INT_MIN
	// sledeci maks. el. u nizu ce biti u stvari nas median

	for (int i = 0; i < 4; i++)
	{
		int ind = __sec_reduce_max_ind(nizMed[:]);
		nizMed[ind] = INT_MIN;
	}
		
	int median = __sec_reduce_max(nizMed[:]);	

	return median;
}

/**
* @brief Pomocna funkcija za racunanja mediana za svaki el. segmenta kolone
*
* @param inBuffer ulazni bafer
* @param outBuffer izlazni bafer
* @param width sirina slike
* @param start pocetak segmenta kolone koju obradjujemo
* @param kraj segmenta kolone koju obradjujemo
*/
void filter_parallel_med_spawned(int* inBuffer, int* outBuffer, int width, int start, int end)
{
	for (int i = 1; i < width - 1; i++)
	{
		for (int j = start; j < end; j++)
		{
			outBuffer[j * width + i] = getParallelMedian(inBuffer, width, j, i);
		}
	}
}

/**
* @brief Parallel version of median filter.
* 
* @param inBuffer buffer of input image
* @param outBuffer buffer of output image
* @param width image width
* @param height image height
*/
void filter_parallel_med(int* inBuffer, int* outBuffer, int width, int height)
{
	int start = 1;
	int end = height / __NUM_OF_SPAWNS__;

	for (int i = 0; i < __NUM_OF_SPAWNS__; i++)
	{
		// parametri (int* inBuffer, int* outBuffer, int width, int start, int end)
		cilk_spawn filter_parallel_med_spawned(inBuffer, outBuffer, width, start, end - 1);
		start = end - 1;
		end += height / __NUM_OF_SPAWNS__ - 1; // -1 zbog obrade zadnjeg el u segmentu
	}

	filter_parallel_med_spawned(inBuffer, outBuffer, width, start, height - 1); // heigth - 1 zbog obrade zadnjeg reda

	cilk_sync;
}


/**
* @brief Print program usage.
*/
void usage()
{
	cout << "ERROR: call program like DigitalImageProcessing.exe input.bmp outputSerialAvg.bmp"
		"outputParallelAvg.bmp outputSerialMed.bmp outputParallelMed.bmp." << endl;
}


int main(int argc, char* argv[])
{
	if (argc != __ARG_NUM__)
	{
		usage();
		return 0;
	}

	BitmapRawConverter inputFile(argv[1]);

	unsigned int width = inputFile.getWidth();
	unsigned int height = inputFile.getHeight();

	BitmapRawConverter outputFileSerialAvg(width, height);
	BitmapRawConverter outputFileParallelAvg(width, height);
	BitmapRawConverter outputFileSerialMed(width, height);
	BitmapRawConverter outputFileParallelMed(width, height);

	unsigned long long start_tick;
	unsigned long long end_tick;


	// SERIAL VERSION - MOVING AVERAGE FILTER
	cout << "Running serial version of moving average filter" << endl;
	start_tick = cilk_getticks();
	filter_serial_avg(inputFile.getBuffer(), outputFileSerialAvg.getBuffer(), width, height);
	end_tick = cilk_getticks();
	cout << "Ticks: " << end_tick - start_tick << endl;
	outputFileSerialAvg.pixelsToBitmap(argv[2]); // saves the result in a file


	// PARALLEL VERSION - MOVING AVERAGE FILTER
	cout << "Running parallel version of moving average filter" << endl;
	start_tick = cilk_getticks();
	// TODO: IMPLEMENT filter_parallel_avg FUNCTION
	filter_parallel_avg(inputFile.getBuffer(), outputFileParallelAvg.getBuffer(), width, height);
	end_tick = cilk_getticks();
	cout << "Ticks: " << end_tick - start_tick << endl;
	outputFileParallelAvg.pixelsToBitmap(argv[3]); // saves the result in a file


	// SERIAL VERSION - MEDIAN FILTER
	cout << "Running serial version of median filter" << endl;
	start_tick = cilk_getticks();
	// TODO: IMPLEMENT filter_serial_med FUNCTION
	filter_serial_med(inputFile.getBuffer(), outputFileSerialMed.getBuffer(), width, height);
	end_tick = cilk_getticks();
	cout << "Ticks: " << end_tick - start_tick << endl;
	outputFileSerialMed.pixelsToBitmap(argv[4]); // saves the result in a file


	// PARALLEL VERSION - MEDIAN FILTER
	cout << "Running parallel version of median filter" << endl;
	start_tick = cilk_getticks();
	// TODO: IMPLEMENT filter_parallel_med FUNCTION
	filter_parallel_med(inputFile.getBuffer(), outputFileParallelMed.getBuffer(), width, height);
	end_tick = cilk_getticks();
	cout << "Ticks: " << end_tick - start_tick << endl;
	outputFileParallelMed.pixelsToBitmap(argv[5]); // saves the result in a file

	return 0;
}
