/*
 *	Copyright (c) 2023-2026, Signaloid.
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

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <uxhw.h>

#include "common.h"

#ifdef NO_OS_AVAILABLE
 #include "simulation_common.h"
enum
{
	kCommonConstantMaxCharsPerPrintMessage = 4128, /* Sized for maximum byte array of TTR256 */
};
#endif

/**
 *	@brief	Read data from a CSV file. Data entries are either numbers or Ux-values.
 *
 *	@param	inputFilePath		path to CSV file to read from
 *	@param	expectedHeaders		array of headers that should be in the CSV data
 *	@param	inputDistributions	array of input distributions to be obtained from the read CSV data
 *	@param	inputDistributionsType	specifies whether input data are single-precision or double-precision floating-point values
 *	@param	numberOfDistributions	size of `inputDistributions` _and_ `expectedHeaders` arrays
 *	@return				`kCommonConstantReturnTypeError` on error, `kCommonConstantReturnTypeSuccess` on success
 */
static CommonConstantReturnType
readInputDistributionsFromCSV(
	const char *                inputFilePath,
	const char *const *         expectedHeaders,
	void *                      inputDistributions,
	FloatingPointVariableType   inputDistributionsType,
	size_t                      numberOfDistributions);

/**
 *	@brief	Write Ux-valued data variables to a CSV file.
 *
 *	@param	outputFilePath			path to the output CSV file to write to
 *	@param	outputVariables			array of output distributions whose Ux representations we will write to the output CSV
 *	@param	outputVariablesType		specifies whether output data are single-precision or double-precision floating-point values
 *	@param	outputVariableNames		names of output distributions whose Ux representations we will write to the output CSV
 *	@param	numberOfOutputDistributions	number of output distributions whose Ux representations we will write to the output CSV
 *	@return					`kCommonConstantReturnTypeError` on error, `kCommonConstantReturnTypeSuccess` on success
 */
static CommonConstantReturnType
writeOutputDistributionsToCSV(
	const char *                outputFilePath,
	const void *                outputVariables,
	FloatingPointVariableType   outputVariablesType,
	const char *const *         outputVariableNames,
	size_t                      numberOfOutputDistributions);

/*
 *	The implementation function is borrowed from googletest and prevents the compiler from
 *	optimising out the functionally redundant code. See
 *	https://github.com/google/benchmark/blob/d8254bb9eb5f6deeddee639d0b27347e186e0a84/include/benchmark/benchmark.h#L314C7-L314C7
 */
void
doNotOptimize(void * value)
{
	__asm__ volatile ("" : : "g" (value) : "memory");

	return;
}

void
fatal(const char * fmt, ...)
{
	va_list argptr;

	va_start(argptr, fmt);
	vfprintf(stderr, fmt, argptr);
	fprintf(stderr, "\n");
	va_end(argptr);

	exit(EXIT_FAILURE);
}

/*
 *	Parse an integer at the start of `str` and set `*end` to the first character after it.
 */
static CommonConstantReturnType
parseIntAndFindEnd(const char * str, int *  out, char * *  end)
{
	assert(str != NULL);
	assert(out != NULL);
	assert(end != NULL);

	long tmp = 0;

	errno   = 0;
	tmp     = strtol(str, end, 10);

	if (errno == ERANGE)
	{
		/*
		 *	There was an integer but it was out of range.
		 */
		return kCommonConstantReturnTypeError;
	}

	if (*end == str)
	{
		/*
		 *	No integer found in `str`.
		 */
		return kCommonConstantReturnTypeError;
	}

	if ((tmp < INT_MIN) || (INT_MAX < tmp))
	{
		/*
		 *	Integer was in range for `long` but not for `int`.
		 */
		return kCommonConstantReturnTypeError;
	}

	*out = (int) tmp;

	return kCommonConstantReturnTypeSuccess;
}

CommonConstantReturnType
parseIntChecked(const char * str, int *  out)
{
	char * end = NULL;

	return parseIntAndFindEnd(str, out, &end);
}

CommonConstantReturnType
parseFloatChecked(const char * str, float *  out)
{
	assert(str != NULL);
	assert(out != NULL);

	char *  end = NULL;
	float   tmp = 0;

	errno   = 0;
	tmp     = strtof(str, &end);

	if ((errno == ERANGE) && isinf(tmp))
	{
		/*
		 *	There was a number but it overflowed. Underflow also sets ERANGE, but the result
		 *	(a subnormal number or zero) is the closest representable value, so it is accepted.
		 */
		return kCommonConstantReturnTypeError;
	}

	if (end == str)
	{
		/*
		 *	No double found in `str`.
		 */
		return kCommonConstantReturnTypeError;
	}

	*out = tmp;

	return kCommonConstantReturnTypeSuccess;
}

CommonConstantReturnType
parseDoubleChecked(const char * str, double *  out)
{
	assert(str != NULL);
	assert(out != NULL);

	char *  end = NULL;
	double  tmp = 0;

	errno   = 0;
	tmp     = strtod(str, &end);

	if ((errno == ERANGE) && isinf(tmp))
	{
		/*
		 *	There was a number but it overflowed. Underflow also sets ERANGE, but the result
		 *	(a subnormal number or zero) is the closest representable value, so it is accepted.
		 */
		return kCommonConstantReturnTypeError;
	}

	if (end == str)
	{
		/*
		 *	No double found in `str`.
		 */
		return kCommonConstantReturnTypeError;
	}

	*out = tmp;

	return kCommonConstantReturnTypeSuccess;
}

/*
 *	Like isspace from libc but without undefined behaviour if t is negative.
 */
static bool
safeIsspace(char t)
{
	return isspace((unsigned char) t);
}

/*
 *	Like `parseIntChecked`, but rejects trailing characters other than whitespace.
 *	For example "1e6" and "3abc" are errors rather than 1 and 3.
 */
static CommonConstantReturnType
parseWholeIntChecked(const char * str, int *  out)
{
	char * end = NULL;

	if (parseIntAndFindEnd(str, out, &end) != kCommonConstantReturnTypeSuccess)
	{
		return kCommonConstantReturnTypeError;
	}

	while (safeIsspace(*end))
	{
		end++;
	}

	return (*end == '\0') ? kCommonConstantReturnTypeSuccess : kCommonConstantReturnTypeError;
}

/*
 *	Reads one line of `fp` into `buffer` and drops the newline. A final line without a newline is
 *	read the same way. Sets `*endOfFile` if there are no more lines.
 *
 *	Unlike `fgets`, this counts the bytes it reads. So it finds a NUL character anywhere in the
 *	line, including in a final line without a newline. Returns `kCommonConstantReturnTypeError`
 *	if the line holds a NUL character or does not fit in `buffer`.
 */
static CommonConstantReturnType
readCSVLine(FILE * fp, char *  buffer, size_t bufferSize, bool *  endOfFile)
{
	size_t  lineLength  = 0;
	int     character   = fgetc(fp);

	*endOfFile = (character == EOF);

	while ((character != EOF) && (character != '\n'))
	{
		if (character == '\0')
		{
			fprintf(stderr, "Error: A line of the input CSV file contains a NUL character.\n");

			return kCommonConstantReturnTypeError;
		}

		if (lineLength == bufferSize - 1)
		{
			fprintf(
				stderr,
				"Error: A line of the input CSV file is longer than the maximum "
				"of %zu characters.\n",
				bufferSize - 1
			);

			return kCommonConstantReturnTypeError;
		}

		buffer[lineLength] = (char) character;
		lineLength++;
		character = fgetc(fp);
	}

	buffer[lineLength] = '\0';

	return kCommonConstantReturnTypeSuccess;
}

/*
 *	Like `strtok_r(..., ",", ...)`, but keeps empty fields, so "a,,b" gives "a", "" and "b".
 *	Returns `NULL` once all fields have been returned.
 */
static char *
nextCSVField(char * * cursor)
{
	char * field = *cursor;

	if (field == NULL)
	{
		return NULL;
	}

	char * comma = strchr(field, ',');

	if (comma == NULL)
	{
		*cursor = NULL;
	}
	else
	{
		*comma  = '\0';
		*cursor = comma + 1;
	}

	return field;
}

/*
 *	A CSV entry of '-', optionally followed by whitespace, marks a missing value.
 */
static bool
isMissingCSVEntry(const char * token)
{
	if (token[0] != '-')
	{
		return false;
	}

	for (size_t ii = 1; token[ii] != '\0'; ii++)
	{
		if (!safeIsspace(token[ii]))
		{
			return false;
		}
	}

	return true;
}

static CommonConstantReturnType
validateInputDistributionCSVHeader(
	char *              actualHeaderRow,
	const char *const * expectedHeaders,
	size_t              numberOfExpectedHeaders)
{
	assert(actualHeaderRow != NULL);
	assert(expectedHeaders != NULL);

	char *  cursor      = actualHeaderRow;
	size_t  columnCount = 0;
	char *  token       = nextCSVField(&cursor);

	while (token)
	{
		if (columnCount == numberOfExpectedHeaders)
		{
			fprintf(stderr, "Error: The input CSV data has more than expected header values\n");

			return kCommonConstantReturnTypeError;
		}

		assert(expectedHeaders[columnCount] != NULL);

		size_t expectedHeaderLength = strlen(expectedHeaders[columnCount]);

		/*
		 *	Trim leading whitespace
		 */
		while (safeIsspace(*token))
		{
			token++;
		}

		/*
		 *	Validate that the token starts with the expected header.
		 */
		if (strncmp(token, expectedHeaders[columnCount], expectedHeaderLength) != 0)
		{
			fprintf(
				stderr,
				"Error: Column %zu of the input CSV should have header '%s' but has header '%s'\n",
				columnCount,
				expectedHeaders[columnCount],
				token
			);

			return kCommonConstantReturnTypeError;
		}

		/*
		 *	Validate that the token ends with only whitespace (no more text)
		 */
		{
			size_t  tokenLength         = strlen(token);
			size_t  tokenSuffixIndex    = expectedHeaderLength;

			for (size_t ii = tokenSuffixIndex; ii < tokenLength; ++ii)
			{
				/*
				 *	Check that this suffix character is whitespace.
				 */
				if (!safeIsspace(token[ii]))
				{
					fprintf(
						stderr,
						"Error: Column %zu of the input CSV should have header '%s' "
						"but has header '%s' (trailing characters)\n",
						columnCount,
						expectedHeaders[columnCount],
						token
					);

					return kCommonConstantReturnTypeError;
				}
			}
		}

		token = nextCSVField(&cursor);
		columnCount++;
	}

	if (columnCount != numberOfExpectedHeaders)
	{
		fprintf(stderr, "Error: The input CSV data has less than expected header values\n");

		return kCommonConstantReturnTypeError;
	}

	return kCommonConstantReturnTypeSuccess;
}

CommonConstantReturnType
readInputFloatDistributionsFromCSV(
	const char *        inputFilePath,
	const char *const * expectedHeaders,
	float *             inputDistributions,
	size_t              numberOfDistributions)
{
	return readInputDistributionsFromCSV(
		inputFilePath,
		expectedHeaders,
		(void *) inputDistributions,
		kFloatingPointVariableTypeFloat,
		numberOfDistributions
	);
}

CommonConstantReturnType
readInputDoubleDistributionsFromCSV(
	const char *        inputFilePath,
	const char *const * expectedHeaders,
	double *            inputDistributions,
	size_t              numberOfDistributions)
{
	return readInputDistributionsFromCSV(
		inputFilePath,
		expectedHeaders,
		(void *) inputDistributions,
		kFloatingPointVariableTypeDouble,
		numberOfDistributions
	);
}

static CommonConstantReturnType
readInputDistributionsFromCSV(
	const char *                inputFilePath,
	const char *const *         expectedHeaders,
	void *                      inputDistributions,
	FloatingPointVariableType   inputDistributionsType,
	size_t                      numberOfDistributions)
{
	if (numberOfDistributions == 0)
	{
		return kCommonConstantReturnTypeSuccess;
	}

	assert(inputFilePath);
	assert(inputDistributions);
	assert(expectedHeaders);

	CommonConstantReturnType    returnCode                  = kCommonConstantReturnTypeError;
	float * *                   inputFloatSampleValues      = NULL;
	double * *                  inputDoubleSampleValues     = NULL;
	float *                     inputFloatDistributions     = NULL;
	double *                    inputDoubleDistributions    = NULL;
	char *                      buffer              = NULL;
	char *                      token               = NULL;
	char *                      cursor              = NULL;
	int64_t                     rowCount            = -1;
	size_t                      columnCount         = 0;
	size_t *                    sampleCounts        = NULL;
	bool *                      uxColumns           = NULL;
	FILE *                      fp                  = NULL;
	float                       parsedFloatValue    = 0;
	double                      parsedDoubleValue   = 0;

	switch (inputDistributionsType)
	{
		case kFloatingPointVariableTypeFloat:
		{
			inputFloatDistributions = (float *) inputDistributions;

			inputFloatSampleValues = (float * *) checkedCalloc(numberOfDistributions, sizeof(float *), __FILE__, __LINE__);

			for (size_t ii = 0; ii < numberOfDistributions; ii++)
			{
				inputFloatSampleValues[ii] = (float *) checkedCalloc(
					kCommonConstantMaxNumberOfInputSamples,
					sizeof(float),
					__FILE__,
					__LINE__
				);
			}

			break;
		}

		case kFloatingPointVariableTypeDouble:
		{
			inputDoubleDistributions = (double *) inputDistributions;

			inputDoubleSampleValues = (double * *) checkedCalloc(numberOfDistributions, sizeof(double *), __FILE__, __LINE__);

			for (size_t ii = 0; ii < numberOfDistributions; ii++)
			{
				inputDoubleSampleValues[ii] = (double *) checkedCalloc(
					kCommonConstantMaxNumberOfInputSamples,
					sizeof(double),
					__FILE__,
					__LINE__
				);
			}

			break;
		}

		case kFloatingPointVariableTypeUnknown:
		default:
		{
			fatal("inputDistributionsType must be specified");
		}
	}

	buffer          = (char *) checkedCalloc(kCommonConstantMaxCharsPerLine, sizeof(char), __FILE__, __LINE__);
	uxColumns       = (bool *) checkedCalloc(numberOfDistributions, sizeof(bool), __FILE__, __LINE__);
	sampleCounts    = (size_t *) checkedCalloc(numberOfDistributions, sizeof(size_t), __FILE__, __LINE__);

	if (strcmp(inputFilePath, "stdin"))
	{
		fp = fopen(inputFilePath, "r");

		if (fp == NULL)
		{
			fprintf(stderr, "Error: Cannot open the file %s.\n", inputFilePath);
			returnCode = kCommonConstantReturnTypeError;
			goto cleanup;
		}
	}
	else
	{
		/*
		 *	Pipeline not supported
		 */
		fprintf(
			stderr,
			"Error: Pipeline mode not implemented. "
			"Please use the '-i' command-line argument option.\n"
		);

		returnCode = kCommonConstantReturnTypeError;
		goto cleanup;
	}

	/*
	 *	Read the lines into buffer.
	 */
	while (true)
	{
		bool endOfFile = false;

		if (readCSVLine(fp, buffer, kCommonConstantMaxCharsPerLine, &endOfFile) != kCommonConstantReturnTypeSuccess)
		{
			returnCode = kCommonConstantReturnTypeError;
			goto cleanup;
		}

		if (endOfFile)
		{
			break;
		}

		/*
		 *	Skip blank lines, such as an empty line at the end of the file.
		 */
		if (buffer[strspn(buffer, " \t\r\n\v\f")] == '\0')
		{
			continue;
		}

		/*
		 *	Validate the row containing field/column names.
		 */
		if (rowCount == -1)
		{
			/*
			 *	Skip a UTF-8 byte order mark, which some tools (e.g. Excel) write at the start
			 *	of the file.
			 */
			const char *    byteOrderMark       = "\xEF\xBB\xBF";
			const size_t    byteOrderMarkLength = strlen(byteOrderMark);
			char *          headerRow           = buffer;

			if (strncmp(headerRow, byteOrderMark, byteOrderMarkLength) == 0)
			{
				headerRow += byteOrderMarkLength;
			}

			if (validateInputDistributionCSVHeader(headerRow, expectedHeaders, numberOfDistributions) != 0)
			{
				returnCode = kCommonConstantReturnTypeError;
				goto cleanup;
			}

			rowCount++;
			continue;
		}
		else if (rowCount >= kCommonConstantMaxNumberOfInputSamples)
		{
			fprintf(
				stderr,
				"Error: The input CSV file has too many rows (the maximum is %" PRId64 ").\n",
				rowCount
			);

			returnCode = kCommonConstantReturnTypeError;
			goto cleanup;
		}

		columnCount = 0;

		cursor  = buffer;
		token   = nextCSVField(&cursor);

		while (token)
		{
			/*
			 *	Trim leading whitespace
			 */
			while (safeIsspace((unsigned char) *token))
			{
				token++;
			}

			if (columnCount == numberOfDistributions)
			{
				fprintf(
					stderr,
					"Error: The input CSV data has more than the expected entries "
					"at data row %" PRIi64 ".\n",
					rowCount
				);

				returnCode = kCommonConstantReturnTypeError;
				goto cleanup;
			}

			const bool shouldIgnore = isMissingCSVEntry(token);

			/*
			 *	A Ux value column holds its Ux value in data row 0 and '-' in all later rows.
			 */
			if (uxColumns[columnCount] && !shouldIgnore)
			{
				fprintf(
					stderr,
					"Error: The input CSV data at row %" PRId64 " and column %zu should be '-' "
					"because the column holds a Ux value (was '%s').\n",
					rowCount,
					columnCount,
					token
				);

				returnCode = kCommonConstantReturnTypeError;
				goto cleanup;
			}

			/*
			 *	Only parse this value if this is not a Ux value column.
			 */
			if (!uxColumns[columnCount])
			{
				if (strstr(token, "Ux") != NULL)
				{
					if (rowCount != 0)
					{
						fprintf(
							stderr,
							"Error: The input CSV data at row %" PRId64 " and column %zu "
							"is a Ux value. Ux values are only allowed in data row 0.\n",
							rowCount,
							columnCount
						);

						returnCode = kCommonConstantReturnTypeError;
						goto cleanup;
					}

					uxColumns[columnCount] = true;
				}

				if (inputDistributionsType == kFloatingPointVariableTypeFloat)
				{
					if (shouldIgnore)
					{
						/*
						 *	Ignore this entry
						 */
						inputFloatSampleValues[columnCount][sampleCounts[columnCount]] = 0.0;
					}
					else if (parseFloatChecked(token, &parsedFloatValue) != kCommonConstantReturnTypeSuccess)
					{
						fprintf(
							stderr,
							"Error: The input CSV data at row %" PRId64 " and column %zu "
							"is not a valid number (was '%s').\n",
							rowCount,
							columnCount,
							token
						);

						returnCode = kCommonConstantReturnTypeError;
						goto cleanup;
					}
					else
					{
						inputFloatSampleValues[columnCount][sampleCounts[columnCount]] = parsedFloatValue;
						sampleCounts[columnCount]++;
					}
				}
				else
				{
					if (shouldIgnore)
					{
						/*
						 *	Ignore this entry
						 */
						inputDoubleSampleValues[columnCount][sampleCounts[columnCount]] = 0.0;
					}
					else if (parseDoubleChecked(token, &parsedDoubleValue) != kCommonConstantReturnTypeSuccess)
					{
						fprintf(
							stderr,
							"Error: The input CSV data at row %" PRId64 " and column %zu "
							"is not a valid number (was '%s').\n",
							rowCount,
							columnCount,
							token
						);

						returnCode = kCommonConstantReturnTypeError;
						goto cleanup;
					}
					else
					{
						inputDoubleSampleValues[columnCount][sampleCounts[columnCount]] = parsedDoubleValue;
						sampleCounts[columnCount]++;
					}
				}
			}

			token = nextCSVField(&cursor);
			columnCount++;
		}

		if (columnCount != numberOfDistributions)
		{
			fprintf(
				stderr,
				"Error: The input CSV data has less than expected entries at data "
				"row %" PRId64 ".\n",
				rowCount
			);

			returnCode = kCommonConstantReturnTypeError;
			goto cleanup;
		}

		rowCount++;
	}

	/*
	 *	An empty file never reaches the header validation above.
	 */
	if (rowCount == -1)
	{
		fprintf(stderr, "Error: The input CSV file %s is empty.\n", inputFilePath);
		returnCode = kCommonConstantReturnTypeError;
		goto cleanup;
	}

	/*
	 *	Building a distribution from zero samples is undefined, so every column needs at least one value.
	 */
	for (size_t ii = 0; ii < numberOfDistributions; ii++)
	{
		if (sampleCounts[ii] == 0)
		{
			fprintf(
				stderr,
				"Error: The input CSV data has no values in column '%s'.\n",
				expectedHeaders[ii]
			);

			returnCode = kCommonConstantReturnTypeError;
			goto cleanup;
		}
	}

	/*
	 *	Get distributions from collected sample values.
	 */
	if (inputDistributionsType == kFloatingPointVariableTypeFloat)
	{
		for (size_t ii = 0; ii < numberOfDistributions; ii++)
		{
			if (uxColumns[ii])
			{
				inputFloatDistributions[ii] = inputFloatSampleValues[ii][0];
			}
			else
			{
				inputFloatDistributions[ii] = UxHwFloatDistFromSamples(inputFloatSampleValues[ii], sampleCounts[ii]);
			}
		}
	}
	else
	{
		for (size_t ii = 0; ii < numberOfDistributions; ii++)
		{
			if (uxColumns[ii])
			{
				inputDoubleDistributions[ii] = inputDoubleSampleValues[ii][0];
			}
			else
			{
				inputDoubleDistributions[ii] = UxHwDoubleDistFromSamples(inputDoubleSampleValues[ii], sampleCounts[ii]);
			}
		}
	}

	returnCode = kCommonConstantReturnTypeSuccess;

cleanup:

	if ((fp != NULL) && (fp != stdin))
	{
		fclose(fp);
	}

	free(buffer);

	if (inputFloatSampleValues != NULL)
	{
		for (size_t ii = 0; ii < numberOfDistributions; ii++)
		{
			free(inputFloatSampleValues[ii]);
		}

		free(inputFloatSampleValues);
	}

	if (inputDoubleSampleValues != NULL)
	{
		for (size_t ii = 0; ii < numberOfDistributions; ii++)
		{
			free(inputDoubleSampleValues[ii]);
		}

		free(inputDoubleSampleValues);
	}
	free(sampleCounts);
	free(uxColumns);

	return returnCode;
}

CommonConstantReturnType
writeOutputFloatDistributionsToCSV(
	const char *        outputFilePath,
	const float *       outputVariables,
	const char *const * outputVariableNames,
	size_t              numberOfOutputDistributions)
{
	return writeOutputDistributionsToCSV(
		outputFilePath,
		(const void *) outputVariables,
		kFloatingPointVariableTypeFloat,
		outputVariableNames,
		numberOfOutputDistributions
	);
}

CommonConstantReturnType
writeOutputDoubleDistributionsToCSV(
	const char *        outputFilePath,
	const double *      outputVariables,
	const char *const * outputVariableNames,
	size_t              numberOfOutputDistributions)
{
	return writeOutputDistributionsToCSV(
		outputFilePath,
		(const void *) outputVariables,
		kFloatingPointVariableTypeDouble,
		outputVariableNames,
		numberOfOutputDistributions
	);
}

/*
 *	Flush `fp`, close it unless it is `stdout`, and report whether any write to it failed.
 *	Failed writes set the stream's error indicator. Buffered data is only written out by `fflush` or `fclose`.
 */
static CommonConstantReturnType
finishWritingFile(FILE * fp, const char * filePath)
{
	bool writeFailed = (fflush(fp) != 0) || (ferror(fp) != 0);

	if ((fp != stdout) && (fclose(fp) != 0))
	{
		writeFailed = true;
	}

	if (writeFailed)
	{
		fprintf(stderr, "Error: Could not write to %s.\n", filePath);

		return kCommonConstantReturnTypeError;
	}

	return kCommonConstantReturnTypeSuccess;
}

CommonConstantReturnType
writeOutputDistributionsToCSV(
	const char *                outputFilePath,
	const void *                outputVariables,
	FloatingPointVariableType   outputVariablesType,
	const char *const *         outputVariableNames,
	size_t                      numberOfOutputDistributions)
{
	FILE *      fp = NULL;
	float *     outputFloatVariables;
	double *    outputDoubleVariables;

	if (strcmp(outputFilePath, "stdout"))
	{
		fp = fopen(outputFilePath, "w");

		if (fp == NULL)
		{
			fprintf(stderr, "Error: Cannot open the file %s.\n", outputFilePath);

			return kCommonConstantReturnTypeError;
		}
	}
	else
	{
		fp = stdout;
	}

	for (size_t ii = 0; ii < numberOfOutputDistributions; ii++)
	{
		fprintf(fp, "%s", outputVariableNames[ii]);

		if (ii != numberOfOutputDistributions - 1)
		{
			fprintf(fp, ", ");
		}
	}

	fprintf(fp, "\n");

	switch (outputVariablesType)
	{
		case kFloatingPointVariableTypeFloat:
		{
			outputFloatVariables = (float *) outputVariables;

			for (size_t ii = 0; ii < numberOfOutputDistributions; ii++)
			{
				fprintf(fp, "%e", outputFloatVariables[ii]);

				if (ii != numberOfOutputDistributions - 1)
				{
					fprintf(fp, ", ");
				}
			}

			break;
		}

		case kFloatingPointVariableTypeDouble:
		{
			outputDoubleVariables = (double *) outputVariables;

			for (size_t ii = 0; ii < numberOfOutputDistributions; ii++)
			{
				fprintf(fp, "%le", outputDoubleVariables[ii]);

				if (ii != numberOfOutputDistributions - 1)
				{
					fprintf(fp, ", ");
				}
			}

			break;
		}

		case kFloatingPointVariableTypeUnknown:
		default:
		{
			fatal("outputVariablesType must be specified");
		}
	}
	fprintf(fp, "\n");

	return finishWritingFile(fp, outputFilePath);
}

/*
 *	Write a string to stdout with JSON string-content escaping per RFC 8259.
 *	Does not emit the surrounding double quotes; the caller is responsible for those.
 *	A NULL input is treated as an empty string.
 */
static void
printJSONEscapedString(const char * s)
{
	if (s == NULL)
	{
		return;
	}

	for (const unsigned char * p = (const unsigned char *) s; *p != '\0'; p++)
	{
		switch (*p)
		{
			case '"':  printf("\\\""); break;

			case '\\': printf("\\\\"); break;

			case '\b': printf("\\b");  break;

			case '\f': printf("\\f");  break;

			case '\n': printf("\\n");  break;

			case '\r': printf("\\r");  break;

			case '\t': printf("\\t");  break;

			default:
			{
				if (*p < 0x20)
				{
					printf("\\u%04x", *p);
				}
				else
				{
					/*
					 *	Pass through printable ASCII and UTF-8 continuation bytes unchanged.
					 */
					putchar(*p);
				}
				break;
			}
		}
	}

	return;
}

void
printJSONVariables(JSONVariable * jsonVariables, size_t count, const char *  description)
{
	/*
	 *	Print JSON outputs.
	 */
	printf("{\n");
	printf("\t\"description\": \"");
	printJSONEscapedString(description);
	printf("\",\n");
	printf("\t\"plots\": [\n");

	for (size_t ii = 0; ii < count; ii++)
	{
		printf("\t\t{\n");

		/*
		 *	We include this property in the JSON for backwards compatibility.
		 */
		printf("\t\t\t\"variableID\": \"");
		printJSONEscapedString(jsonVariables[ii].variableSymbol);
		printf("\",\n");
		printf("\t\t\t\"variableSymbol\": \"");
		printJSONEscapedString(jsonVariables[ii].variableSymbol);
		printf("\",\n");
		printf("\t\t\t\"variableDescription\": \"");
		printJSONEscapedString(jsonVariables[ii].variableDescription);
		printf("\",\n");

		printf("\t\t\t\"values\": [\n");

		for (size_t jj = 0; jj < jsonVariables[ii].size; jj++)
		{
			switch (jsonVariables[ii].type)
			{
				case kJSONVariableTypeDouble:
				{
					printf("\t\t\t\t\"%f\"", jsonVariables[ii].values.asDouble[jj]);
					break;
				}

				case kJSONVariableTypeFloat:
				{
					printf("\t\t\t\t\"%f\"", jsonVariables[ii].values.asFloat[jj]);
					break;
				}

				case kJSONVariableTypeDoubleParticle:
				{
					printf("\t\t\t\t\"% " SignaloidParticleModifier "f\"", jsonVariables[ii].values.asDouble[jj]);
					break;
				}

				case kJSONVariableTypeFloatParticle:
				{
					printf("\t\t\t\t\"% " SignaloidParticleModifier "f\"", jsonVariables[ii].values.asFloat[jj]);
					break;
				}

				case kJSONVariableTypeUnknown:
				default:
				{
					fatal("kJSONVariableTypeUnknown must be specified");
				}
			}

			if (jj < (jsonVariables[ii].size - 1))
			{
				printf(", \n");
			}
			else
			{
				printf("\n");
			}
		}

		printf("\t\t\t],\n");

		printf("\t\t\t\"particleValues\": [\n");

		for (size_t jj = 0; jj < jsonVariables[ii].size; jj++)
		{
			switch (jsonVariables[ii].type)
			{
				case kJSONVariableTypeDouble:
				case kJSONVariableTypeDoubleParticle:
				{
					printf(
						"\t\t\t\t% " SignaloidParticleModifier "f",
						jsonVariables[ii].values.asDouble[jj]
					);
					break;
				}

				case kJSONVariableTypeFloat:
				case kJSONVariableTypeFloatParticle:
				{
					printf(
						"\t\t\t\t% " SignaloidParticleModifier "f",
						jsonVariables[ii].values.asFloat[jj]
					);
					break;
				}

				case kJSONVariableTypeUnknown:
				default:
				{
					fatal("kJSONVariableTypeUnknown must be specified");
				}
			}

			if (jj < (jsonVariables[ii].size - 1))
			{
				printf(", \n");
			}
			else
			{
				printf("\n");
			}
		}

		printf("\t\t\t],\n");

		printf("\t\t\t\"meanValues\": [\n");

		for (size_t jj = 0; jj < jsonVariables[ii].size; jj++)
		{
			switch (jsonVariables[ii].type)
			{
				case kJSONVariableTypeDouble:
				{
					printf(
						"\t\t\t\t% " SignaloidParticleModifier "f",
						UxHwDoubleNthMoment(jsonVariables[ii].values.asDouble[jj], 1)
					);
					break;
				}

				case kJSONVariableTypeFloat:
				{
					printf(
						"\t\t\t\t% " SignaloidParticleModifier "f",
						UxHwFloatNthMoment(jsonVariables[ii].values.asFloat[jj], 1)
					);
					break;
				}

				case kJSONVariableTypeDoubleParticle:
				{
					/*
					 *	A particle is a degenerate distribution at its own
					 *	value, so its mean is simply the particle value.
					 */
					printf(
						"\t\t\t\t% " SignaloidParticleModifier "f",
						jsonVariables[ii].values.asDouble[jj]
					);
					break;
				}

				case kJSONVariableTypeFloatParticle:
				{
					/*
					 *	A particle is a degenerate distribution at its own
					 *	value, so its mean is simply the particle value.
					 */
					printf(
						"\t\t\t\t% " SignaloidParticleModifier "f",
						jsonVariables[ii].values.asFloat[jj]
					);
					break;
				}

				case kJSONVariableTypeUnknown:
				default:
				{
					fatal("kJSONVariableTypeUnknown must be specified");
				}
			}

			if (jj < (jsonVariables[ii].size - 1))
			{
				printf(", \n");
			}
			else
			{
				printf("\n");
			}
		}

		printf("\t\t\t],\n");

		printf("\t\t\t\"stdDevValues\": [\n");

		for (size_t jj = 0; jj < jsonVariables[ii].size; jj++)
		{
			switch (jsonVariables[ii].type)
			{
				case kJSONVariableTypeDouble:
				{
					printf(
						"\t\t\t\t% " SignaloidParticleModifier "f",
						sqrt(UxHwDoubleNthMoment(jsonVariables[ii].values.asDouble[jj], 2))
					);
					break;
				}

				case kJSONVariableTypeFloat:
				{
					printf(
						"\t\t\t\t% " SignaloidParticleModifier "f",
						sqrt(UxHwFloatNthMoment(jsonVariables[ii].values.asFloat[jj], 2))
					);
					break;
				}

				case kJSONVariableTypeFloatParticle:
				case kJSONVariableTypeDoubleParticle:
				{
					/*
					 *	A particle is a degenerate distribution at its own
					 *	value, so its variance (and therefore stddev) is 0.
					 */
					printf("\t\t\t\t% " SignaloidParticleModifier "f", 0.0);
					break;
				}

				case kJSONVariableTypeUnknown:
				default:
				{
					fatal("kJSONVariableTypeUnknown must be specified");
				}
			}

			if (jj < (jsonVariables[ii].size - 1))
			{
				printf(", \n");
			}
			else
			{
				printf("\n");
			}
		}

		printf("\t\t\t]\n");

		printf("\t\t}");

		if (ii < count - 1)
		{
			printf(",");
		}
		printf("\n");
	}

	printf("\t]\n");
	printf("}\n");

	return;
}

void
populateJSONVariableStruct(
	JSONVariable *  jsonVariable,
	double *        outputVariableValues,
	const char *    outputVariableDescription,
	size_t          outputSelect,
	size_t          numberOfOutputVariableValues)
{
	snprintf(jsonVariable->variableSymbol, kCommonConstantMaxCharsPerJSONVariableSymbol, "outputVariables[%zu]", outputSelect);
	snprintf(jsonVariable->variableDescription, kCommonConstantMaxCharsPerJSONVariableDescription, "%s", outputVariableDescription);
	jsonVariable->values    = (JSONVariablePointer){ .asDouble = outputVariableValues };
	jsonVariable->type      = kJSONVariableTypeDouble;
	jsonVariable->size      = numberOfOutputVariableValues;

	return;
}

/*
 *	A `NULL` `outputVariableTypes` means all outputs are distributions.
 */
static bool
isOutputVariableScalar(const kOutputVariableTypeIndex * outputVariableTypes, size_t outputIndex)
{
	return (outputVariableTypes != NULL) && (outputVariableTypes[outputIndex] == kOutputVariableTypeScalar);
}

/*
 *	In Monte Carlo mode, `monteCarloOutputSamples` holds the samples of one output only.
 *	So when all outputs are selected, at most one of them may be a distribution.
 */
static void
checkMonteCarloOutputSelection(
	const CommonCommandLineArguments *  arguments,
	const kOutputVariableTypeIndex *    outputVariableTypes,
	size_t                              numberOfOutputVariables)
{
	size_t numberOfDistributionOutputs = 0;

	if (!arguments->isMonteCarloMode || (arguments->outputSelect != numberOfOutputVariables))
	{
		return;
	}

	for (size_t ii = 0; ii < numberOfOutputVariables; ii++)
	{
		if (!isOutputVariableScalar(outputVariableTypes, ii))
		{
			numberOfDistributionOutputs++;
		}
	}

	if (numberOfDistributionOutputs > 1)
	{
		fatal(
			"Error: Monte Carlo mode cannot print %zu distribution outputs at once. "
			"Please select a single output (-S option).",
			numberOfDistributionOutputs
		);
	}

	return;
}

void
printJSONFormattedOutput(
	CommonCommandLineArguments *        arguments,
	double *                            monteCarloOutputSamples,
	double *                            outputVariables,
	const char * *                      outputVariableDescriptions,
	size_t                              numberOfOutputVariables,
	const char *                        description,
	const kOutputVariableTypeIndex *    outputVariableTypes)
{
	JSONVariable * jsonVariables = (JSONVariable *) checkedCalloc(
		numberOfOutputVariables,
		sizeof(JSONVariable),
		__FILE__,
		__LINE__
	);
	size_t  outputSelectLowerBound;
	size_t  outputSelectUpperBound;
	size_t  outputSelect;

	determineIndexRangeOfSelectedOutputs(
		arguments,
		numberOfOutputVariables,
		&outputSelectLowerBound,
		&outputSelectUpperBound
	);
	checkMonteCarloOutputSelection(arguments, outputVariableTypes, numberOfOutputVariables);

	for (outputSelect = outputSelectLowerBound; outputSelect < outputSelectUpperBound; outputSelect++)
	{
		/*
		 *	If in Monte Carlo mode and the output is not a scalar, `pointerToOutputVariable` points to the
		 *	beginning of the `monteCarloOutputSamples` array, which has length
		 *	`arguments->numberOfMonteCarloIterations`. Else, it points to the single entry of the
		 *	`outputVariables` to be used.
		 */
		const bool  isOutputScalar          = isOutputVariableScalar(outputVariableTypes, outputSelect);
		const bool  isOutputMonteCarlo      = arguments->isMonteCarloMode && !isOutputScalar;
		double *    pointerToOutputVariable = &outputVariables[outputSelect];

		if (isOutputMonteCarlo)
		{
			pointerToOutputVariable = monteCarloOutputSamples;
		}

		populateJSONVariableStruct(
			&jsonVariables[outputSelect],
			pointerToOutputVariable,
			outputVariableDescriptions[outputSelect],
			outputSelect,
			isOutputMonteCarlo ? arguments->numberOfMonteCarloIterations : 1
		);
	}

	printJSONVariables(
		&jsonVariables[outputSelectLowerBound],
		outputSelectUpperBound - outputSelectLowerBound,
		description
	);

	free(jsonVariables);

	return;
}

static void
setDefaultCommandLineArgumentValues(CommonCommandLineArguments * arguments)
{
	assert(arguments != NULL);

	*arguments = (CommonCommandLineArguments) {
		.outputFilePath                 = "",
		.inputFilePath                  = "",
		.isWriteToFileEnabled           = false,
		.isTimingEnabled                = false,
		.numberOfMonteCarloIterations   = 1,
		.outputSelect                   = 0,
		.isOutputSelected               = false,
		.isVerbose              = false,
		.isInputFromFileEnabled = false,
		.isOutputJSONMode       = false,
		.isHelpEnabled          = false,
		.isBenchmarkingMode     = false,
		.isMonteCarloMode       = false,
		.isSingleShotExecution  = true,
	};

	return;
}

static bool
getOptIsNewlib(void)
{
#ifdef _NEWLIB_VERSION

	return true;

#endif
#ifdef MOCK_NEWLIB_VERSION

	return true;

#endif

	return false;
}

/*
 *	In every other POSIX libc `getopt_long_only` parses long options starting with "-" (as well
 *	 as "--"). However, newlibc's `getopt_long_only` (inexplicably) requires long options to
 *	 start with either "+" or "--". From the source:
 *
 *	> The function getopt_long_only() is identical to getopt_long(), except that a plus sign `+'
 *	> can introduce long options as well as `--'.
 *
 *	We work around this inconsistency by transforming options before `getopt_long_only` parses
 *	them to convert a single leading '-' into a '+'. Returns the string it changed (or `NULL`),
 *	so the caller can change it back after parsing.
 */
static char *
modifyNextArgvForNewLibc(char *const argv[])
{
	if (getOptIsNewlib())
	{
		char * arg = argv[optind == 0 ? 1 : optind];

		if ((arg != NULL) && (arg[0] == '-') && (arg[1] != '-'))
		{
			arg[0] = '+';

			return arg;
		}
	}

	return NULL;
}

static void
checkDuplicates(
	struct option * currentOpts,
	size_t          currentOptsSize,
	const char *    newOption)
{
	for (size_t ii = 0; ii < currentOptsSize; ii++)
	{
		if (strcmp(currentOpts[ii].name, newOption) == 0)
		{
			fatal("Internal Error: Duplicate option '%s'", newOption);
		}
	}

	return;
}

static void
constructLongOptions(
	DemoOption *    demoOpts,
	size_t          demoOptsSize,
	struct option * optionsOut,
	size_t          optionsOutSize)
{
	/*
	 *	Two long opts per demo opt plus zero entry.
	 */
	assert(optionsOutSize >= 2 * demoOptsSize + 1);
	assert(demoOptsSize < INT_MAX);

	size_t outIndex = 0;

	/*
	 *	Demo options go first (value of option is index into opts)
	 */
	for (size_t ii = 0; ii < demoOptsSize; ii++)
	{
		int hasArg  = demoOpts[ii].hasArg ? required_argument : no_argument;
		int val     = (int) ii;

		if ((demoOpts[ii].opt == NULL) && (demoOpts[ii].optAlternative == NULL))
		{
			fatal("Internal Error: Options for demo missing both option names (index %zu).", ii);
		}

		if (demoOpts[ii].opt != NULL)
		{
			checkDuplicates(optionsOut, outIndex, demoOpts[ii].opt);

			optionsOut[outIndex] = (struct option) {
				.name       = demoOpts[ii].opt,
				.has_arg    = hasArg,
				.flag       = NULL,
				.val        = val,
			};
			outIndex++;
		}

		if (demoOpts[ii].optAlternative != NULL)
		{
			checkDuplicates(optionsOut, outIndex, demoOpts[ii].optAlternative);

			optionsOut[outIndex] = (struct option) {
				.name       = demoOpts[ii].optAlternative,
				.has_arg    = hasArg,
				.flag       = NULL,
				.val        = val,
			};
			outIndex++;
		}
	}

	/*
	 *	Put the final zero entry in
	 */
	optionsOut[outIndex] = (struct option) { ZERO_STRUCT_INIT };
	outIndex++;

	return;
}

static CommonConstantReturnType
parseArgsCoreImplementation(
	int             argc,
	char *const     argv[],
	DemoOption *    options,
	size_t          optionsSize)
{
	size_t          longOptionsSize = 0;
	struct option * longOptions     = NULL;
	bool            error           = false;
	int             longIndex;
	int             previousOptInd;
	int             opt;

	/*
	 *	Set initial values for `foundOpt` and `foundArg`.
	 */
	for (size_t ii = 0; ii < optionsSize; ++ii)
	{
		if (options[ii].foundOpt != NULL)
		{
			*(options[ii].foundOpt) = false;
		}

		if (options[ii].foundArg != NULL)
		{
			*(options[ii].foundArg) = NULL;
		}
	}

	/*
	 *	At most two longopt's per demo option (opt and optAlternative) plus zero entry.
	 */
	longOptionsSize = optionsSize * 2 + 1;
	longOptions     = (struct option *) checkedCalloc(longOptionsSize, sizeof(struct option), __FILE__, __LINE__);

	constructLongOptions(options, optionsSize, longOptions, longOptionsSize);

	optind  = 0;
	opterr  = 0;

	/*
	 *	From the getopt man page:
	 *
	 *	> The variable optind is the index of the next element to be processed in argv.
	 *
	 *	Therefore, we remember the previous value of optind which tells us the index of the
	 *	_current_ index being processed. This is very useful for error messages.
	 */
	previousOptInd = 1;

	/*
	 *	Strings changed by `modifyNextArgvForNewLibc`, to be changed back after parsing.
	 */
	char * *    modifiedArgs            = (char * *) checkedCalloc((size_t) argc, sizeof(char *), __FILE__, __LINE__);
	size_t      numberOfModifiedArgs    = 0;

	while (!error)
	{
		longIndex = -1;

		/*
		 *	Rather than support both long and short opts (and maintain code to handle
		 *	both), we only support long opts. However, we use the `_only` form of
		 *	`getopt_long` which allows users to specify long opts with a single dash.
		 *
		 *	Therefore, specify a single character long opt and this will parse short
		 *	opts as well! The only catch is that you cannot group short opts: something
		 *	like `-To` must be instead written `-T -o`. I think this is a fine
		 *	compromise to make.
		 */
		/*
		 *	glibc, musl and newlib disagree on "--", on a lone "-" and on arguments that start
		 *	with '+'. Handle them here, before `modifyNextArgvForNewLibc`, so that all behave like
		 *	glibc. "--" ends the options. "-" and "+..." are non-option arguments, which also end
		 *	the options.
		 */
		const int       nextIndex   = (optind == 0) ? 1 : optind;
		const char *    nextArg     = (nextIndex < argc) ? argv[nextIndex] : NULL;

		if ((nextArg != NULL) && (strcmp(nextArg, "--") == 0))
		{
			optind = nextIndex + 1;
			break;
		}

		if ((nextArg != NULL) && ((strcmp(nextArg, "-") == 0) || (nextArg[0] == '+')))
		{
			optind = nextIndex;
			break;
		}

		char * modifiedArg = modifyNextArgvForNewLibc(argv);

		if (modifiedArg != NULL)
		{
			modifiedArgs[numberOfModifiedArgs] = modifiedArg;
			numberOfModifiedArgs++;
		}

		/*
		 *	The leading '+' stops parsing at the first non-option argument, on both glibc and newlib.
		 *	Without it, both reorder `argv` differently, and `previousOptInd` would not point at the
		 *	option being processed. Any non-option argument is an error anyway (see after the loop).
		 */
		opt = getopt_long_only(argc, argv, "+:", longOptions, &longIndex);

		if (opt == -1)
		{
			break;
		}

		if (longIndex == -1)
		{
			/*
			 *	Error case.
			 */
			switch (opt)
			{
				case '?':
				{
					/*
					 *	The invalid long opts can be found at
					 *	`argv[previousOptInd]`. We know the first character
					 *	when printing will (prior to
					 *	`modifyNextArgvForNewLibc`) be '-' so we print that
					 *	explicitly.
					 */
					fprintf(stderr, "Error: Invalid option: '-%s' provided.\n", argv[previousOptInd] + 1);
					break;
				}

				case ':':
				{
					/*
					 *	The long opts that is missing an argument can be
					 *	found at `argv[optind - 1]`. We know the first
					 *	character when printing will (prior to
					 *	`modifyNextArgvForNewLibc`) be '-' so we print that
					 *	explicitly.
					 */
					fprintf(stderr, "Error: Option '-%s' is missing mandatory argument.\n", argv[optind - 1] + 1);
					break;
				}

				default:
				{
					fprintf(stderr, "Error: Unhandled getopt error return value: -%c.\n", opt);
					break;
				}
			}

			error = true;
			break;
		}

		assert(opt >= 0);
		assert((size_t) opt < optionsSize);

		/*
		 *	`getopt_long_only` also accepts unique prefixes of option names (e.g. "-m" for "-M"'s long
		 *	form), and newlib ignores "=value" on options without an argument. So check that the option
		 *	being processed, `argv[previousOptInd]`, names the matched option exactly.
		 */
		const char *    optionName  = longOptions[longIndex].name;
		const char *    optionText  = argv[previousOptInd] + strspn(argv[previousOptInd], "-+");
		const size_t    nameLength  = strcspn(optionText, "=");
		const bool      hasValue    = (optionText[nameLength] == '=');
		const bool      isPrefix    = (strncmp(optionText, optionName, nameLength) == 0);
		const bool      isExactName = isPrefix && (optionName[nameLength] == '\0');

		if (!isExactName || (hasValue && !options[opt].hasArg))
		{
			fprintf(stderr, "Error: Invalid option: '-%s' provided.\n", argv[previousOptInd] + 1);
			error = true;
			break;
		}

		/*
		 *	glibc and musl accept an empty value such as "-k=", but newlib reports it as missing.
		 *	Treat it as missing everywhere.
		 */
		if (hasValue && (optionText[nameLength + 1] == '\0'))
		{
			fprintf(
				stderr,
				"Error: Option '-%s' is missing mandatory argument.\n",
				argv[previousOptInd] + 1
			);
			error = true;
			break;
		}

		if (options[opt].foundOpt != NULL)
		{
			*(options[opt].foundOpt) = true;
		}

		if (options[opt].hasArg)
		{
			if (optarg == NULL)
			{
				fatal("Internal error: option -%s missing argument.", longOptions[longIndex].name);
			}

			if (options[opt].foundArg != NULL)
			{
				*(options[opt].foundArg) = optarg;
			}
		}

		previousOptInd = optind;
	}

	if (!error && optind < argc)
	{
		fprintf(stderr, "Error: Unexpected argument '%s'\n", argv[optind]);
		error = true;
	}

	/*
	 *	Undo `modifyNextArgvForNewLibc`, so the caller's `argv` is unchanged.
	 */
	for (size_t ii = 0; ii < numberOfModifiedArgs; ii++)
	{
		modifiedArgs[ii][0] = '-';
	}

	free(modifiedArgs);
	free(longOptions);

	return error ? kCommonConstantReturnTypeError : kCommonConstantReturnTypeSuccess;
}


/*
 *	Count the options before the terminating zero entry. A `NULL` `options` has no options.
 */
static size_t
countDemoOptions(const DemoOption * options)
{
	size_t count = 0;

	if (options == NULL)
	{
		return 0;
	}

	while ((options[count].opt != NULL) || (options[count].optAlternative != NULL))
	{
		count++;
	}

	return count;
}

static CommonConstantReturnType
concatAndParseArgs(
	int             argc,
	char *const     argv[],
	DemoOption *    demoSpecificOptions,
	DemoOption *    commonOptions)
{
	const size_t    demoSpecificOptionsSize = countDemoOptions(demoSpecificOptions);
	const size_t    commonOptionsSize       = countDemoOptions(commonOptions);
	const size_t    concatOptionsSize       = demoSpecificOptionsSize + commonOptionsSize;
	DemoOption *    concatOptions           = (DemoOption *) checkedCalloc(
		concatOptionsSize,
		sizeof(DemoOption),
		__FILE__,
		__LINE__
	);

	/*
	 *	`memcpy` must not be passed a `NULL` source, even with a size of 0.
	 */
	if (demoSpecificOptionsSize > 0)
	{
		memcpy(
			concatOptions,
			demoSpecificOptions,
			sizeof(DemoOption) * demoSpecificOptionsSize
		);
	}

	memcpy(
		concatOptions + demoSpecificOptionsSize,
		commonOptions,
		sizeof(DemoOption) * commonOptionsSize
	);

	const CommonConstantReturnType ret = parseArgsCoreImplementation(argc, argv, concatOptions, concatOptionsSize);

	free(concatOptions);

	return ret;
}

CommonConstantReturnType
parseArgs(
	int                             argc,
	char *const                     argv[],
	CommonCommandLineArguments *    arguments,
	DemoOption *                    demoSpecificOptions)
{
	assert(arguments != NULL);

	setDefaultCommandLineArgumentValues(arguments);

	const char *    inputArg                = NULL;
	const char *    outputArg               = NULL;
	const char *    outputSelectArg         = NULL;
	const char *    multipleExecutionsArg   = NULL;
	DemoOption      commonOptions[]         = {
		{ "input",               "i", true,  &inputArg,              NULL                           },
		{ "output",              "o", true,  &outputArg,             NULL                           },
		{ "select-output",       "S", true,  &outputSelectArg,       NULL                           },
		{ "time",                "T", false, NULL,                   &arguments->isTimingEnabled    },
		{ "multiple-executions", "M", true,  &multipleExecutionsArg, NULL                           },
		{ "verbose",             "v", false, NULL,                   &arguments->isVerbose          },
		{ "json",                "j", false, NULL,                   &arguments->isOutputJSONMode   },
		{ "help",                "h", false, NULL,                   &arguments->isHelpEnabled      },
		{ "benchmarking",        "b", false, NULL,                   &arguments->isBenchmarkingMode },
		{ ZERO_STRUCT_INIT }
	};

	if (concatAndParseArgs(argc, argv, demoSpecificOptions, commonOptions) != kCommonConstantReturnTypeSuccess)
	{
		return kCommonConstantReturnTypeError;
	}

	if (inputArg != NULL)
	{
		int ret = snprintf(arguments->inputFilePath, kCommonConstantMaxCharsPerFilepath, "%s", inputArg);

		if ((ret < 0) || (ret >= kCommonConstantMaxCharsPerFilepath))
		{
			fprintf(stderr, "Error: Could not read input file path from command-line arguments.\n");

			return kCommonConstantReturnTypeError;
		}
		else
		{
			arguments->isInputFromFileEnabled = true;
		}
	}

	if (outputArg != NULL)
	{
		int ret = snprintf(arguments->outputFilePath, kCommonConstantMaxCharsPerFilepath, "%s", outputArg);

		if ((ret < 0) || (ret >= kCommonConstantMaxCharsPerFilepath))
		{
			fprintf(stderr, "Error: Could not read output file path from command-line arguments.\n");

			return kCommonConstantReturnTypeError;
		}
		else
		{
			arguments->isWriteToFileEnabled = true;
		}
	}

	if (outputSelectArg != NULL)
	{
		int outputSelect;
		int ret = parseWholeIntChecked(outputSelectArg, &outputSelect);

		if (ret != kCommonConstantReturnTypeSuccess)
		{
			fprintf(stderr, "Error: The output selected must be an integer.\n");

			return kCommonConstantReturnTypeError;
		}
		else if (outputSelect < 0)
		{
			fprintf(stderr, "Error: The output selected must be non-negative.\n");

			return kCommonConstantReturnTypeError;
		}
		else
		{
			arguments->outputSelect     = outputSelect;
			arguments->isOutputSelected = true;
		}
	}

	if (multipleExecutionsArg != NULL)
	{
		int multipleExecutions;
		int ret = parseWholeIntChecked(multipleExecutionsArg, &multipleExecutions);

		if (ret != kCommonConstantReturnTypeSuccess)
		{
			fprintf(stderr, "Error: The number of multiple executions must be an integer.\n");

			return kCommonConstantReturnTypeError;
		}
		else if (multipleExecutions <= 0)
		{
			fprintf(stderr, "Error: The number of multiple executions must be positive.\n");

			return kCommonConstantReturnTypeError;
		}
		else
		{
			arguments->numberOfMonteCarloIterations = multipleExecutions;

			arguments->isMonteCarloMode         = true;
			arguments->isTimingEnabled          = true;
			arguments->isSingleShotExecution    = false;
		}
	}

	/*
	 *	JSON output mode and benchmarking mode are not compatible.
	 */
	if ((arguments->isOutputJSONMode) && (arguments->isBenchmarkingMode))
	{
		fprintf(stderr, "Error: Output JSON mode and benchmarking mode are not compatible. Please choose only one.\n");

		return kCommonConstantReturnTypeError;
	}

	return kCommonConstantReturnTypeSuccess;
}

void
printCommonUsage(void)
{
	fprintf(stderr, "Usage: Valid command-line arguments are:\n");
	fprintf(
		stderr,
		"\t[-i, --input <Path to input CSV file : str>] (Read inputs from file.)\n"
		"\t[-o, --output <Path to output CSV file : str>] (Specify the output file.)\n"
		"\t[-S, --select-output <output : int>] (Compute 0-indexed output, by default 0. "
		"Set to the number of outputs to compute all outputs.)\n"
		"\t[-M, --multiple-executions <Number of executions : int (Default: 1)>] "
		"(Repeatedly execute kernel for benchmarking. Also enables timing.)\n"
		"\t[-T, --time] (Timing mode: Times and prints the timing of the kernel execution.)\n"
		"\t[-v, --verbose] (Verbose mode: Prints extra information about demo execution.)\n"
		"\t[-b, --benchmarking] (Benchmarking mode: Generate outputs in format for benchmarking.)\n"
		"\t[-j, --json] (Print output in JSON format.)\n"
		"\t[-h, --help] (Display this help message.)\n"
	);

	return;
}

MeanAndVariance
calculateMeanAndVarianceOfFloatSamples(
	const float *   dataArray,
	size_t          dataArraySize)
{
	if (dataArraySize == 0)
	{
		fatal("Error: Cannot compute mean and variance of 0 samples.");
	}

	double  sum = 0;
	double  sumOfSquaredDeviations = 0;

	for (size_t ii = 0; ii < dataArraySize; ii++)
	{
		sum += dataArray[ii];
	}

	/*
	 *	Summing squared deviations from the mean in a second pass avoids the cancellation in
	 *	E[x^2] - E[x]^2, which can give a negative variance.
	 */
	const double mean = sum / dataArraySize;

	for (size_t ii = 0; ii < dataArraySize; ii++)
	{
		const double deviation = dataArray[ii] - mean;

		sumOfSquaredDeviations += deviation * deviation;
	}

	return (MeanAndVariance)
		   {
			   .mean        = mean,
			   .variance    = sumOfSquaredDeviations / dataArraySize,
		   };
}

MeanAndVariance
calculateMeanAndVarianceOfDoubleSamples(
	const double *  dataArray,
	size_t          dataArraySize)
{
	if (dataArraySize == 0)
	{
		fatal("Error: Cannot compute mean and variance of 0 samples.");
	}

	double  sum = 0;
	double  sumOfSquaredDeviations = 0;

	for (size_t ii = 0; ii < dataArraySize; ii++)
	{
		sum += dataArray[ii];
	}

	/*
	 *	Summing squared deviations from the mean in a second pass avoids the cancellation in
	 *	E[x^2] - E[x]^2, which can give a negative variance.
	 */
	const double mean = sum / dataArraySize;

	for (size_t ii = 0; ii < dataArraySize; ii++)
	{
		const double deviation = dataArray[ii] - mean;

		sumOfSquaredDeviations += deviation * deviation;
	}

	return (MeanAndVariance)
		   {
			   .mean        = mean,
			   .variance    = sumOfSquaredDeviations / dataArraySize,
		   };
}

static int
compareFloats(const void * a, const void * b)
{
	const float first   = *(const float *) a;
	const float second  = *(const float *) b;

	return ((int) (first > second)) - ((int) (first < second));
}

static int
compareDoubles(const void * a, const void * b)
{
	const double    first   = *(const double *) a;
	const double    second  = *(const double *) b;

	return ((int) (first > second)) - ((int) (first < second));
}

float
calculatePercentageQuantileOfFloatSamples(
	const float *   dataArray,
	float           quantilePercentage,
	size_t          dataArraySize)
{
	/*
	 *	A quantile needs at least one sample and a `quantilePercentage` in [0, 1]. This also rejects NaN.
	 */
	if ((dataArraySize == 0) || !((quantilePercentage >= 0) && (quantilePercentage <= 1)))
	{
		fatal(
			"Error: Cannot compute quantile %f of %zu samples. "
			"The quantile must be in [0, 1] and there must be at least one sample.",
			(double) quantilePercentage,
			dataArraySize
		);
	}

	/*
	 *	NaN samples have no order, so `qsort` cannot sort them.
	 */
	for (size_t ii = 0; ii < dataArraySize; ii++)
	{
		if (isnan(dataArray[ii]))
		{
			fatal("Error: Cannot compute quantile. Sample %zu is NaN.", ii);
		}
	}

	/*
	 *	Make a copy for sorting.
	 */
	float * dataArrayCopy = (float *) checkedMalloc(
		dataArraySize * sizeof(float),
		__FILE__,
		__LINE__
	);

	memcpy(dataArrayCopy, dataArray, dataArraySize * sizeof(float));

	/*
	 *	Sort the copy. `dataArray` is not modified.
	 */
	qsort(dataArrayCopy, dataArraySize, sizeof(float), compareFloats);

	/*
	 *	Determine the relevant index. A quantile of 1, or rounding up, maps to the last sample.
	 */
	size_t index = (size_t) (quantilePercentage * dataArraySize);

	if (index >= dataArraySize)
	{
		index = dataArraySize - 1;
	}

	/*
	 *	Return the value at that index.
	 */
	const float quantile = dataArrayCopy[index];

	free(dataArrayCopy);

	return quantile;
}

double
calculatePercentageQuantileOfDoubleSamples(
	const double *  dataArray,
	double          quantilePercentage,
	size_t          dataArraySize)
{
	/*
	 *	A quantile needs at least one sample and a `quantilePercentage` in [0, 1]. This also rejects NaN.
	 */
	if ((dataArraySize == 0) || !((quantilePercentage >= 0) && (quantilePercentage <= 1)))
	{
		fatal(
			"Error: Cannot compute quantile %f of %zu samples. "
			"The quantile must be in [0, 1] and there must be at least one sample.",
			(double) quantilePercentage,
			dataArraySize
		);
	}

	/*
	 *	NaN samples have no order, so `qsort` cannot sort them.
	 */
	for (size_t ii = 0; ii < dataArraySize; ii++)
	{
		if (isnan(dataArray[ii]))
		{
			fatal("Error: Cannot compute quantile. Sample %zu is NaN.", ii);
		}
	}

	/*
	 *	Make a copy for sorting.
	 */
	double * dataArrayCopy = (double *) checkedMalloc(
		dataArraySize * sizeof(double),
		__FILE__,
		__LINE__
	);

	memcpy(dataArrayCopy, dataArray, dataArraySize * sizeof(double));

	/*
	 *	Sort the copy. `dataArray` is not modified.
	 */
	qsort(dataArrayCopy, dataArraySize, sizeof(double), compareDoubles);

	/*
	 *	Determine the relevant index. A quantile of 1, or rounding up, maps to the last sample.
	 */
	size_t index = (size_t) (quantilePercentage * dataArraySize);

	if (index >= dataArraySize)
	{
		index = dataArraySize - 1;
	}

	/*
	 *	Return the value at that index.
	 */
	const double quantile = dataArrayCopy[index];

	free(dataArrayCopy);

	return quantile;
}

void
calculateMeanAndVarianceOfMultiDimensionalFloatSamples(
	float * *   dataArray,
	size_t      dataArrayRows,
	size_t      dataArrayColumns,
	float *     meanValueArray,
	float *     varianceArray)
{
	if (dataArrayRows == 0)
	{
		fatal("Error: Cannot compute mean and variance of 0 samples.");
	}

	for (size_t jj = 0; jj < dataArrayColumns; jj++)
	{
		double  sum = 0;
		double  sumOfSquaredDeviations = 0;

		for (size_t ii = 0; ii < dataArrayRows; ii++)
		{
			sum += dataArray[ii][jj];
		}

		/*
		 *	Summing squared deviations from the mean in a second pass avoids the cancellation in
		 *	E[x^2] - E[x]^2, which can give a negative variance.
		 */
		const double mean = sum / dataArrayRows;

		for (size_t ii = 0; ii < dataArrayRows; ii++)
		{
			const double deviation = dataArray[ii][jj] - mean;

			sumOfSquaredDeviations += deviation * deviation;
		}

		meanValueArray[jj]  = (float) mean;
		varianceArray[jj]   = (float) (sumOfSquaredDeviations / dataArrayRows);
	}

	return;
}

void
calculateMeanAndVarianceOfMultiDimensionalDoubleSamples(
	double * *  dataArray,
	size_t      dataArrayRows,
	size_t      dataArrayColumns,
	double *    meanValueArray,
	double *    varianceArray)
{
	if (dataArrayRows == 0)
	{
		fatal("Error: Cannot compute mean and variance of 0 samples.");
	}

	for (size_t jj = 0; jj < dataArrayColumns; jj++)
	{
		double  sum = 0;
		double  sumOfSquaredDeviations = 0;

		for (size_t ii = 0; ii < dataArrayRows; ii++)
		{
			sum += dataArray[ii][jj];
		}

		/*
		 *	Summing squared deviations from the mean in a second pass avoids the cancellation in
		 *	E[x^2] - E[x]^2, which can give a negative variance.
		 */
		const double mean = sum / dataArrayRows;

		for (size_t ii = 0; ii < dataArrayRows; ii++)
		{
			const double deviation = dataArray[ii][jj] - mean;

			sumOfSquaredDeviations += deviation * deviation;
		}

		meanValueArray[jj]  = mean;
		varianceArray[jj]   = sumOfSquaredDeviations / dataArrayRows;
	}

	return;
}



CommonConstantReturnType
saveMonteCarloFloatDataToDataDotOutFile(
	const float *   benchmarkingDataSamples,
	uint64_t        cpuTimeElapsedMicroSeconds,
	size_t          numberOfMonteCarloIterations)
{
	FILE * fp = fopen("data.out", "w");

	if (fp == NULL)
	{
		fatal("Could not open monte carlo output file");
	}

	fprintf(fp, "%" PRIu64 "\n", cpuTimeElapsedMicroSeconds);

	for (size_t ii = 0; ii < numberOfMonteCarloIterations; ii++)
	{
		fprintf(fp, "%.20f\n", benchmarkingDataSamples[ii]);
	}

	return finishWritingFile(fp, "data.out");
}

CommonConstantReturnType
saveMonteCarloFloatMultidimensionalDataToDataDotOutFile(
	float * *   benchmarkingDataSamples,
	uint64_t    cpuTimeElapsedMicroSeconds,
	size_t      numberOfOutputVariables,
	size_t      numberOfMonteCarloIterations)
{
	FILE * fp = fopen("data.out", "w");

	if (fp == NULL)
	{
		fatal("Could not open monte carlo output file");
	}

	fprintf(fp, "%" PRIu64 "\n", cpuTimeElapsedMicroSeconds);

	for (size_t jj = 0; jj < numberOfMonteCarloIterations; jj++)
	{
		for (size_t ii = 0; ii < numberOfOutputVariables; ii++)
		{
			fprintf(fp, "%.20f", benchmarkingDataSamples[ii][jj]);

			/*
			 *	Print a comma after each output variable sample.
			 *	Print a newline after printing the final output variable.
			 */
			if (ii < numberOfOutputVariables - 1)
			{
				fprintf(fp, ", ");
			}
			else
			{
				fprintf(fp, "\n");
			}
		}
	}

	return finishWritingFile(fp, "data.out");
}

CommonConstantReturnType
saveMonteCarloDoubleDataToDataDotOutFile(
	const double *  benchmarkingDataSamples,
	uint64_t        cpuTimeElapsedMicroSeconds,
	size_t          numberOfMonteCarloIterations)
{
	FILE * fp = fopen("data.out", "w");

	if (fp == NULL)
	{
		fatal("Could not open monte carlo output file");
	}

	fprintf(fp, "%" PRIu64 "\n", cpuTimeElapsedMicroSeconds);

	for (size_t ii = 0; ii < numberOfMonteCarloIterations; ii++)
	{
		fprintf(fp, "%.20lf\n", benchmarkingDataSamples[ii]);
	}

	return finishWritingFile(fp, "data.out");
}

CommonConstantReturnType
saveMonteCarloDoubleMultidimensionalDataToDataDotOutFile(
	double * *  benchmarkingDataSamples,
	uint64_t    cpuTimeElapsedMicroSeconds,
	size_t      numberOfOutputVariables,
	size_t      numberOfMonteCarloIterations)
{
	FILE * fp = fopen("data.out", "w");

	if (fp == NULL)
	{
		fatal("Could not open monte carlo output file");
	}

	fprintf(fp, "%" PRIu64 "\n", cpuTimeElapsedMicroSeconds);

	for (size_t jj = 0; jj < numberOfMonteCarloIterations; jj++)
	{
		for (size_t ii = 0; ii < numberOfOutputVariables; ii++)
		{
			fprintf(fp, "%.20f", benchmarkingDataSamples[ii][jj]);

			/*
			 *	Print a comma after each output variable sample.
			 *	Print a newline after printing the final output variable.
			 */
			if (ii < numberOfOutputVariables - 1)
			{
				fprintf(fp, ", ");
			}
			else
			{
				fprintf(fp, "\n");
			}
		}
	}

	return finishWritingFile(fp, "data.out");
}

void *
checkedMalloc(size_t size, const char *  file, int line)
{
	void * ret = malloc(size);

	if (ret == NULL)
	{
		fatal("malloc() failed to allocate %zu bytes at %s:%d", size, file, line);
	}

	return ret;
}

void *
checkedCalloc(size_t count, size_t size, const char *  file, int line)
{
	void * ret = calloc(count, size);

	if (ret == NULL)
	{
		fatal("calloc() failed to allocate %zu bytes at %s:%d", count * size, file, line);
	}

	return ret;
}


void
determineIndexRangeOfSelectedOutputs(
	CommonCommandLineArguments *    arguments,
	size_t                          numberOfOutputVariables,
	size_t *                        pointerToOutputSelectLowerBound,
	size_t *                        pointerToOutputSelectUpperBound)
{
	/*
	 *	Callers index output arrays with this range, so reject an out-of-range `outputSelect`.
	 */
	if (arguments->outputSelect > numberOfOutputVariables)
	{
		fatal(
			"Error: Output select value (-S option) %zu is greater than the number of outputs %zu.",
			arguments->outputSelect,
			numberOfOutputVariables
		);
	}

	/*
	 *	If `outputSelect` is equal to `numberOfOutputVariables`, index range is the full range.
	 */
	if (arguments->outputSelect == numberOfOutputVariables)
	{
		*pointerToOutputSelectLowerBound    = 0;
		*pointerToOutputSelectUpperBound    = numberOfOutputVariables;
	}
	/*
	 *	Else, index range lower bound is the index of the selected output and the length of range is 1 (single output selected).
	 */
	else
	{
		*pointerToOutputSelectLowerBound    = arguments->outputSelect;
		*pointerToOutputSelectUpperBound    = *pointerToOutputSelectLowerBound + 1;
	}

	return;
}

bool
isSelectedOutputScalar(
	const CommonCommandLineArguments *  arguments,
	const kOutputVariableTypeIndex *    outputVariableTypes,
	size_t                              numberOfOutputVariables)
{
	/*
	 *	If all outputs are selected (`outputSelect` equals `numberOfOutputVariables`), there is no single
	 *	selected output.
	 */
	if (arguments->outputSelect >= numberOfOutputVariables)
	{
		return false;
	}

	return isOutputVariableScalar(outputVariableTypes, arguments->outputSelect);
}

void
printHumanConsumableOutput(
	CommonCommandLineArguments *        arguments,
	size_t                              numberOfOutputVariables,
	double *                            outputVariables,
	const char *                        outputVariableNames[],
	const char *                        outputVariableDescriptions[],
	double *                            monteCarloOutputSamples,
	const kOutputVariableTypeIndex *    outputVariableTypes)
{
	size_t          outputSelectLowerBound;
	size_t          outputSelectUpperBound;
	size_t          outputSelect;
	MeanAndVariance stats;
	double          sampleMin;
	double          sampleMax;

	determineIndexRangeOfSelectedOutputs(
		arguments,
		numberOfOutputVariables,
		&outputSelectLowerBound,
		&outputSelectUpperBound
	);
	checkMonteCarloOutputSelection(arguments, outputVariableTypes, numberOfOutputVariables);

	for (outputSelect = outputSelectLowerBound; outputSelect < outputSelectUpperBound; outputSelect++)
	{
		/*
		 *	Scalar outputs have no Monte Carlo samples, so print them as a single value even in Monte Carlo
		 *	mode.
		 */
		if (arguments->isMonteCarloMode && !isOutputVariableScalar(outputVariableTypes, outputSelect))
		{
			stats = calculateMeanAndVarianceOfDoubleSamples(
				monteCarloOutputSamples,
				arguments->numberOfMonteCarloIterations
			);

			sampleMin   = monteCarloOutputSamples[0];
			sampleMax   = monteCarloOutputSamples[0];

			for (size_t ii = 1; ii < arguments->numberOfMonteCarloIterations; ii++)
			{
				if (monteCarloOutputSamples[ii] < sampleMin)
				{
					sampleMin = monteCarloOutputSamples[ii];
				}

				if (monteCarloOutputSamples[ii] > sampleMax)
				{
					sampleMax = monteCarloOutputSamples[ii];
				}
			}

#ifdef NO_OS_AVAILABLE
			char    tempString[kCommonConstantMaxCharsPerPrintMessage];
			int     written;

			written = snprintf(
				tempString,
				kCommonConstantMaxCharsPerPrintMessage,
				"%s (%s):",
				outputVariableDescriptions[outputSelect],
				outputVariableNames[outputSelect]
			);

			if ((written < 0) || (written >= (int) kCommonConstantMaxCharsPerPrintMessage))
			{
				fputs("Warning: human-consumable output line truncated\n", stderr);
			}
			puts(tempString);
			snprintf(tempString, kCommonConstantMaxCharsPerPrintMessage, "  Mean:     %e", stats.mean);
			puts(tempString);
			snprintf(tempString, kCommonConstantMaxCharsPerPrintMessage, "  Variance: %e", stats.variance);
			puts(tempString);
			snprintf(tempString, kCommonConstantMaxCharsPerPrintMessage, "  Min:      %e", sampleMin);
			puts(tempString);
			snprintf(tempString, kCommonConstantMaxCharsPerPrintMessage, "  Max:      %e", sampleMax);
			puts(tempString);
#else
			printf("%s (%s):\n", outputVariableDescriptions[outputSelect], outputVariableNames[outputSelect]);
			printf("  Mean:     %e\n", stats.mean);
			printf("  Variance: %e\n", stats.variance);
			printf("  Min:      %e\n", sampleMin);
			printf("  Max:      %e\n", sampleMax);
#endif
		}
		else
		{
#ifdef NO_OS_AVAILABLE
			char    tempString[kCommonConstantMaxCharsPerPrintMessage];
			int     written;
			ssize_t resultSize;

			written = snprintf(
				tempString,
				kCommonConstantMaxCharsPerPrintMessage,
				"%s:",
				outputVariableDescriptions[outputSelect]
			);

			if ((written < 0) || (written >= (int) kCommonConstantMaxCharsPerPrintMessage))
			{
				fputs("Warning: human-consumable output line truncated\n", stderr);
			}
			puts(tempString);
			snprintf(tempString, kCommonConstantMaxCharsPerPrintMessage, "%lf", outputVariables[outputSelect]);
			puts(tempString);

			resultSize = UxHwDoubleDistributionToByteArray(
				outputVariables[outputSelect],
				(uint8_t *) (tempString),
				(uint32_t) kCommonConstantMaxCharsPerPrintMessage
			);

			if (resultSize < 0)
			{
				fputs("Warning: could not convert double to byte array\n", stderr);
			}
			else
			{
				puts("  Byte array: ");

				for (ssize_t ii = 0; ii < resultSize; ii++)
				{
					putHexChar(tempString[ii]);
				}

				puts("\r\n");
			}
#else
			printf("%s: %lf\n", outputVariableDescriptions[outputSelect], outputVariables[outputSelect]);
#endif
		}
	}

	return;
}
