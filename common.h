/*
 *	Copyright (c) 2023, Signaloid.
 *
 *	Permission is hereby granted, free of charge, to any person obtaining a copy
 *	of this software and associated documentation files (the "Software"), to deal
 *	in the Software without restriction, including without limitation the rights
 *	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *	copies of the Software, and to permit persons to whom the Software is
 *	furnished to do so, subject to the following conditions:
 *
 *	The above copyright notice and this permission notice shall be included in all
 *	copies or substantial portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *	SOFTWARE.
 */

#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#ifdef __cplusplus
#define ZERO_STRUCT_INIT
#else
#define ZERO_STRUCT_INIT 0
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
	kCommonConstantMaxCharsPerFilepath			= 1024,
	kCommonConstantMaxCharsPerLine				= 1024 * 1024,
	kCommonConstantMaxNumberOfInputSamples			= 10000,
	kCommonConstantMaxCharsPerJSONVariableSymbol		= 256,
	kCommonConstantMaxCharsPerJSONVariableDescription	= 1024,
} CommonConstant;

typedef enum
{
	kCommonConstantReturnTypeError		= 1,
	kCommonConstantReturnTypeSuccess	= 0,
} CommonConstantReturnType;

typedef enum
{
	kFloatingPointVariableTypeUnknown,
	kFloatingPointVariableTypeFloat,
	kFloatingPointVariableTypeDouble,
} FloatingPointVariableType;

typedef enum
{
	kJSONVariableTypeUnknown,
	kJSONVariableTypeFloat,
	kJSONVariableTypeDouble,
	kJSONVariableTypeFloatParticle,
	kJSONVariableTypeDoubleParticle,
} JSONVariableType;

typedef union
{
	const double *		asDouble;
	const float *		asFloat;
} JSONVariablePointer;

typedef struct JSONVariable
{
	char			variableSymbol[kCommonConstantMaxCharsPerJSONVariableSymbol];
	char			variableDescription[kCommonConstantMaxCharsPerJSONVariableDescription];
	JSONVariablePointer	values;
	JSONVariableType	type;
	size_t			size;
} JSONVariable;

/**
 *	@brief	Prevent compiler optimising away an otherwise unused value.
 *	Used for native Monte Carlo benchmarking.
 *
 *	@param	ptr	Pointer to value to force compiler to keep. Must not be `NULL`.
 */
void
doNotOptimize(void *  ptr);


/**
 *	@brief	fatal print and exit.
 *
 *	@param	fmt	error message to print before exit
 *	@param		format args
 */
__attribute__((noreturn))
void	fatal(const char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));

/**
 *	@brief	Read single-precision floating-point data from a CSV file. Data entries are either numbers or Ux-values.
 *
 *	@param	inputFilePath		path to CSV file to read from
 *	@param	expectedHeaders		array of headers that should be in the CSV data
 *	@param	inputDistributions	array of input distributions to be obtained from the read CSV data
 *	@param	numberOfDistributions	size of `inputDistributions` _and_ `expectedHeaders` arrays
 *	@return				`kCommonConstantReturnTypeError` on error, `kCommonConstantReturnTypeSuccess` on success
 */
CommonConstantReturnType
readInputFloatDistributionsFromCSV(
	const char *		inputFilePath,
	const char * const *	expectedHeaders,
	float *			inputDistributions,
	size_t			numberOfDistributions);

/**
 *	@brief	Read double-precision floating-point data from a CSV file. Data entries are either numbers or Ux-values.
 *
 *	@param	inputFilePath		path to CSV file to read from
 *	@param	expectedHeaders		array of headers that should be in the CSV data
 *	@param	inputDistributions	array of input distributions to be obtained from the read CSV data
 *	@param	numberOfDistributions	size of `inputDistributions` _and_ `expectedHeaders` arrays
 *	@return				`kCommonConstantReturnTypeError` on error, `kCommonConstantReturnTypeSuccess` on success
 */
CommonConstantReturnType
readInputDoubleDistributionsFromCSV(
	const char *		inputFilePath,
	const char * const *	expectedHeaders,
	double *		inputDistributions,
	size_t			numberOfDistributions);

/**
 *	@brief	Write Ux-valued data of single-precision floating-point variables to a CSV file.
 *
 *	@param	outputFilePath			path prefix for the output CSV file to write to
 *	@param	outputVariables			array of output distributions whose Ux representations we will write to the output CSV
 *	@param	outputVariableNames		names of output distributions whose Ux representations we will write to the output CSV
 *	@param	numberOfOutputDistributions	number of output distributions whose Ux representations we will write to the output CSV
 *	@return					`kCommonConstantReturnTypeError` on error, `kCommonConstantReturnTypeSuccess` on success
 */
CommonConstantReturnType
writeOutputFloatDistributionsToCSV(
	const char *		outputFilePath,
	const float *		outputVariables,
	const char * const *	outputVariableNames,
	size_t			numberOfOutputDistributions);

/**
 *	@brief	Write Ux-valued data of double-precision floating-point variables to a CSV file.
 *
 *	@param	outputFilePath			path prefix for the output CSV file to write to
 *	@param	outputVariables			array of output distributions whose Ux representations we will write to the output CSV
 *	@param	outputVariableNames		names of output distributions whose Ux representations we will write to the output CSV
 *	@param	numberOfOutputDistributions	number of output distributions whose Ux representations we will write to the output CSV
 *	@return					`kCommonConstantReturnTypeError` on error, `kCommonConstantReturnTypeSuccess` on success
 */
CommonConstantReturnType
writeOutputDoubleDistributionsToCSV(
	const char *		outputFilePath,
	const double *		outputVariables,
	const char * const *	outputVariableNames,
	size_t			numberOfOutputDistributions);

/**
 *	@brief	Print Ux-valued data to `stdout` in JSON format.
 *
 *	@param	jsonVariables	array of JSONVariable items
 *	@param	count		number of JSONVariable items
 *	@param	description	JSON description
 */
void
printJSONVariables(
	JSONVariable *	jsonVariables,
	size_t		count,
	const char *	description);

/**
 *	@brief	Parse an integer at the start of `str`, trailing characters are ignored.
 *
 *	@param	str	String to parse
 *	@param	out	Will be set to parsed value
 *	@return 	`kCommonConstantReturnTypeError` on error, `kCommonConstantReturnTypeSuccess` on success
 */
CommonConstantReturnType
parseIntChecked(
	const char *	str,
	int *		out);

/**
 *	@brief	Parse a single-precision floating-point value at the start of `str`, trailing characters are ignored.
 *
 *	@param	str	String to parse
 *	@param	out	Will be set to parsed value
 *	@return 	`kCommonConstantReturnTypeError` on error, `kCommonConstantReturnTypeSuccess` on success
 */
CommonConstantReturnType
parseFloatChecked(
	const char *	str,
	float *		out);

/**
 *	@brief	Parse a double-precision floating-pointvalue at the start of `str`, trailing characters are ignored.
 *
 *	@param	str	String to parse
 *	@param	out	Will be set to parsed value
 *	@return 	`kCommonConstantReturnTypeError` on error, `kCommonConstantReturnTypeSuccess` on success
 */
CommonConstantReturnType
parseDoubleChecked(
	const char *	str,
	double *	out);

typedef struct
{
	char	outputFilePath[kCommonConstantMaxCharsPerFilepath];
	char	inputFilePath[kCommonConstantMaxCharsPerFilepath];
	bool	isWriteToFileEnabled;
	bool	isTimingEnabled;
	size_t	numberOfMonteCarloIterations;
	size_t	outputSelect;
	bool	isOutputSelected;
	bool	isVerbose;
	bool	isInputFromFileEnabled;
	bool	isOutputJSONMode;
	bool	isHelpEnabled;
	bool	isBenchmarkingMode;
	bool	isMonteCarloMode;
	bool	isSingleShotExecution;
} CommonCommandLineArguments;

typedef struct
{
	/**
	 *	@brief The command line flag (without leading `-` or `--`) for this option.
	 */
	const char *	opt;

	/**
	 *	@brief An alternative command line flag (without leading `-` or `--`) for this option.
	 *
	 *	@details Often used for short options.
	 */
	const char *	optAlternative;

	/**
	 *	@brief Whether this option takes an argument.
	 *
	 *	@details If set, then parsing will fail unless the command line flag has an
	 *	argument.
	 */
	bool		hasArg;

	/**
	 *	@brief Pass pointer to variable in which any argument will be stored.
	 *
	 *	@details May be to `NULL` (e.g. if this option does not have an argument), in which
	 *	case nothing will be written.
	 */
	const char **	foundArg;

	/**
	 *	@brief Pass pointer to variable which will be set to true if option is found.
	 *
	 *	@details May be to `NULL`, in which case nothing will be written.
	 */
	bool *		foundOpt;
} DemoOption;

/**
 *	@brief	Parse command line arguments into `args`.
 *
 * 	@details Grouped short options are not supported. (Use `-W -j` rather than `-Wj`.)
 *
 *	@param	argc			As provided to `main()`
 *	@param	argv			As provided to `main()`
 *	@param	args			Parsed command line arguments are stored here
 *	@param	demoSpecificOptions	Extra command line arguments to parse
 *	@return 			`kCommonConstantReturnTypeError` on error, `kCommonConstantReturnTypeSuccess` on success
 */
CommonConstantReturnType
parseArgs(
	int				argc,
	char *  const			argv[],
	CommonCommandLineArguments *	arguments,
	DemoOption *			demoSpecificOptions);

/**
 *	@brief	Print the common part of the usage to stdout.
 */
void
printCommonUsage(void);

typedef struct
{
	double	mean;
	double	variance;
} MeanAndVariance;

/**
 *	@brief	Caluculate mean and variance of float data.
 *
 *	@param	dataArray	data array for which to compute mean and variance
 *	@param	dataArraySize	number of items in `dataArray`
 *	@return			calculated mean and variance
 */
MeanAndVariance
calculateMeanAndVarianceOfFloatSamples(
	const float *	dataArray,
	size_t		dataArraySize);

/**
 *	@brief	Caluculate mean and variance of double data.
 *
 *	@param	dataArray	data array for which to compute mean and variance
 *	@param	dataArraySize	number of items in `dataArray`
 *	@return			calculated mean and variance
 */
MeanAndVariance
calculateMeanAndVarianceOfDoubleSamples(
	const double *	dataArray,
	size_t		dataArraySize);

/**
 *	@brief	Writes Monte Carlo samples to a file 'data.out'
 *
 *	@param	benchmarkingDataSamples		Array (length `numberOfMonteCarloIterations`) of float Monte Carlo samples
 *	@param	cpuTimeElapsedMicroSeconds	Execution time of kernel
 *	@param	numberOfMonteCarloIterations	Number of samples (one per iteration)
 *	@return	void
 */
void
saveMonteCarloFloatDataToDataDotOutFile(
	const float *	benchmarkingDataSamples,
	uint64_t	cpuTimeElapsedMicroSeconds,
	size_t		numberOfMonteCarloIterations);

/**
 *	@brief	Writes Monte Carlo samples to a file 'data.out'
 *
 *	@param	benchmarkingDataSamples		Array (length `numberOfMonteCarloIterations`) of double Monte Carlo samples
 *	@param	cpuTimeElapsedMicroSeconds	Execution time of kernel
 *	@param	numberOfMonteCarloIterations	Number of samples (one per iteration)
 *	@return	void
 */
void
saveMonteCarloDoubleDataToDataDotOutFile(
	const double *	benchmarkingDataSamples,
	uint64_t	cpuTimeElapsedMicroSeconds,
	size_t		numberOfMonteCarloIterations);

/**
 *	@brief	Call Malloc and abort on allocation failure.
 *
 *	@param	size	Passed to malloc
 *	@param	file	File name to include in error message
 *	@param	line	Line number to include in error message
 *	@return		Pointer as returned by malloc
 */
void *
checkedMalloc(
	size_t		size,
	const char *	file,
	int		line);

/**
 *	@brief	Call Calloc and abort on allocation failure.
 *
 *	@param	size	Passed to calloc
 *	@param	num	Passed to calloc
 *	@param	file	File name to include in error message
 *	@param	line	Line number to include in error message
 *	@return		Pointer as returned by calloc
 */
void *
checkedCalloc(
	size_t		num,
	size_t		size,
	const char *	file,
	int		line);

#ifdef __cplusplus
} /* extern "C" */
#endif
