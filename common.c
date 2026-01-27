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

/**
 *	@brief	Read data from a CSV file. Data entries are wither numbers or Ux-values.
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
	const char *			inputFilePath,
	const char * const *		expectedHeaders,
	void *				inputDistributions,
	FloatingPointVariableType	inputDistributionsType,
	size_t				numberOfDistributions);

/**
 *	@brief	Write Ux-valued data variables to a CSV file.
 *
 *	@param	outputFilePath			path prefix for the output CSV file to write to
 *	@param	outputVariables			array of output distributions whose Ux representations we will write to the output CSV
 *	@param	outputVariablesType		specifies whether output data are single-precision or double-precision floating-point values
 *	@param	outputVariableNames		names of output distributions whose Ux representations we will write to the output CSV
 *	@param	numberOfOutputDistributions	number of output distributions whose Ux representations we will write to the output CSV
 *	@return					`kCommonConstantReturnTypeError` on error, `kCommonConstantReturnTypeSuccess` on success
 */
static CommonConstantReturnType
writeOutputDistributionsToCSV(
	const char *			outputFilePath,
	const void *			outputVariables,
	FloatingPointVariableType	outputVariablesType,
	const char * const *		outputVariableNames,
	size_t				numberOfOutputDistributions);

/*
 *	The implementation function is borrowed from googletest and prevents the compiler from
 *	optimising out the functionally redundant code. See
 *	https://github.com/google/benchmark/blob/d8254bb9eb5f6deeddee639d0b27347e186e0a84/include/benchmark/benchmark.h#L314C7-L314C7
 */
void
doNotOptimize(void *  value)
{
	__asm__ volatile("" : : "g"(value) : "memory");
}

void
fatal(const char *  fmt, ...)
{
	va_list argptr;
	va_start(argptr, fmt);
	vfprintf(stderr, fmt, argptr);
	fprintf(stderr, "\n");
	va_end(argptr);

	exit(EXIT_FAILURE);
}

CommonConstantReturnType
parseIntChecked(const char *  str, int *  out)
{
	assert(str != NULL);
	assert(out != NULL);

	char *	end = NULL;
	long	tmp = 0;

	errno = 0;
	tmp = strtol(str, &end, 10);

	if (errno == ERANGE)
	{
		/*
		 *	There was an integer but it was out of range.
		 */
		return kCommonConstantReturnTypeError;
	}

	if (end == str)
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

	*out = (int)tmp;

	return kCommonConstantReturnTypeSuccess;
}

CommonConstantReturnType
parseFloatChecked(const char *  str, float *  out)
{
	assert(str != NULL);
	assert(out != NULL);

	char *	end = NULL;
	float	tmp = 0;

	errno = 0;
	tmp = strtof(str, &end);

	if (errno == ERANGE)
	{
		/*
		 *	There was an double but it was out of range.
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
parseDoubleChecked(const char *  str, double *  out)
{
	assert(str != NULL);
	assert(out != NULL);

	char *	end = NULL;
	double	tmp = 0;

	errno = 0;
	tmp = strtod(str, &end);

	if (errno == ERANGE)
	{
		/*
		 *	There was an double but it was out of range.
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
	return isspace((unsigned char)t);
}

static CommonConstantReturnType
validateInputDistributionCSVHeader(
	char * 			actualHeaderRow,
	const char * const *	expectedHeaders,
	size_t			numberOfExpectedHeaders)
{
	assert(actualHeaderRow != NULL);
	assert(expectedHeaders != NULL);

	char *		strtokState;
	size_t		columnCount = 0;
	char *		token = strtok_r(actualHeaderRow, ",", &strtokState);

	while (token)
	{
		if (columnCount == numberOfExpectedHeaders)
		{
			fprintf(stderr, "Error: The input CSV data has more than expected header values\n");

			return kCommonConstantReturnTypeError;
		}

		assert(expectedHeaders[columnCount] != NULL);

		size_t	expectedHeaderLength = strlen(expectedHeaders[columnCount]);

		/*
		 *	Trim leading whitespace
		 */
		while(safeIsspace(*token))
		{
			token++;
		}

		/*
		 *	Validate that the token starts with the expected header.
		 */
		if(strncmp(token, expectedHeaders[columnCount], expectedHeaderLength) != 0)
		{
			fprintf(
				stderr,
				"Error: Column %zu of the input CSV should have header '%s' but has header '%s'\n",
				columnCount,
				expectedHeaders[columnCount],
				token);

			return kCommonConstantReturnTypeError;
		}

		/*
		 *	Validate that the token ends with only whitespace (no more text)
		 */
		{
			size_t	tokenLength = strlen(token);
			size_t	tokenSuffixIndex = expectedHeaderLength;

			for (size_t ii = tokenSuffixIndex; ii < tokenLength; ++ii)
			{
				/*
				 *	Check that this suffix character is whitespace.
				 */
				if (!safeIsspace(token[ii]))
				{
					fprintf(
						stderr,
						"Error: Column %zu of the input CSV should have header '%s' but has header '%s' (trailing characters)\n",
						columnCount,
						expectedHeaders[columnCount],
						token);

					return kCommonConstantReturnTypeError;
				}
			}
		}

		token = strtok_r(NULL, ",", &strtokState);
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
	const char *			inputFilePath,
	const char * const *		expectedHeaders,
	float *				inputDistributions,
	size_t				numberOfDistributions)
{
	return readInputDistributionsFromCSV(
						inputFilePath,
						expectedHeaders,
						(void * ) inputDistributions,
						kFloatingPointVariableTypeFloat,
						numberOfDistributions);
}

CommonConstantReturnType
readInputDoubleDistributionsFromCSV(
	const char *			inputFilePath,
	const char * const *		expectedHeaders,
	double *			inputDistributions,
	size_t				numberOfDistributions)
{
	return readInputDistributionsFromCSV(
						inputFilePath,
						expectedHeaders,
						(void * ) inputDistributions,
						kFloatingPointVariableTypeDouble,
						numberOfDistributions);
}

static CommonConstantReturnType
readInputDistributionsFromCSV(
	const char *			inputFilePath,
	const char * const *		expectedHeaders,
	void *				inputDistributions,
	FloatingPointVariableType	inputDistributionsType,
	size_t				numberOfDistributions)
{
	if (numberOfDistributions == 0)
	{
		return kCommonConstantReturnTypeSuccess;
	}

	assert(inputFilePath);
	assert(inputDistributions);
	assert(expectedHeaders);

	CommonConstantReturnType	returnCode = kCommonConstantReturnTypeError;
	float **			inputFloatSampleValues = NULL;
	double **			inputDoubleSampleValues = NULL;
	float *				inputFloatDistributions = NULL;
	double *			inputDoubleDistributions = NULL;
	char *				buffer = NULL;
	char *				token;
	char *				strtokState;
	int64_t				rowCount = -1;
	size_t				columnCount;
	size_t *			sampleCounts = NULL;
	bool *				uxColumns = NULL;
	FILE *				fp = NULL;
	float				parsedFloatValue;
	double				parsedDoubleValue;

	switch (inputDistributionsType)
	{
		case kFloatingPointVariableTypeFloat:
		{
			inputFloatDistributions = (float *) inputDistributions;

			inputFloatSampleValues = (float **)checkedCalloc(numberOfDistributions, sizeof(double *), __FILE__, __LINE__);
			for (size_t ii = 0; ii < numberOfDistributions; ii++)
			{
				inputFloatSampleValues[ii] = (float *)checkedCalloc(kCommonConstantMaxNumberOfInputSamples, sizeof(float), __FILE__, __LINE__);
			}
			break;
		}
		case kFloatingPointVariableTypeDouble:
		{
			inputDoubleDistributions = (double *) inputDistributions;

			inputDoubleSampleValues = (double **)checkedCalloc(numberOfDistributions, sizeof(double *), __FILE__, __LINE__);
			for (size_t ii = 0; ii < numberOfDistributions; ii++)
			{
				inputDoubleSampleValues[ii] = (double *)checkedCalloc(kCommonConstantMaxNumberOfInputSamples, sizeof(double), __FILE__, __LINE__);
			}
			break;
		}
		case kFloatingPointVariableTypeUnknown:
		default:
		{
			fatal("inputDistributionsType must be specified");
		}
	}

	buffer = (char *)checkedCalloc(kCommonConstantMaxCharsPerLine, sizeof(char), __FILE__, __LINE__);
	uxColumns = (bool *)checkedCalloc(numberOfDistributions, sizeof(bool), __FILE__, __LINE__);
	sampleCounts = (size_t *)checkedCalloc(numberOfDistributions, sizeof(size_t), __FILE__, __LINE__);

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
			"Please use the '-i' command-line argument option.\n");

		returnCode = kCommonConstantReturnTypeError;
		goto cleanup;
	}

	/*
	 *	Read the lines into buffer.
	 */
	while (fgets(buffer, kCommonConstantMaxCharsPerLine, fp))
	{
		/*
		 *	Validate the row containing field/column names.
		 */
		if (rowCount == -1)
		{
			if (validateInputDistributionCSVHeader(buffer, expectedHeaders, numberOfDistributions) != 0)
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
				rowCount);

			returnCode = kCommonConstantReturnTypeError;
			goto cleanup;
		}

		columnCount = 0;

		token = strtok_r(buffer, ",", &strtokState);

		while (token)
		{
			/*
			 *	Trim leading whitespace
			 */
			while (safeIsspace((unsigned char)*token))
			{
				token++;
			}

			if (columnCount == numberOfDistributions)
			{
				fprintf(stderr,
					"Error: The input CSV data has more than the expected entries "
					"at data row %" PRIi64 ".\n",
					rowCount);

				returnCode = kCommonConstantReturnTypeError;
				goto cleanup;
			}

			/*
			 *	Only parse this value if this is not a Ux value column.
			 */
			if (!uxColumns[columnCount])
			{
				if ((rowCount == 0) && (strstr(token, "Ux") != NULL))
				{
					uxColumns[columnCount] = true;
				}

				bool	shouldIgnore = false;

				if (token[0] == '-')
				{
					for (size_t ii = 1; true; ii++)
					{
						if (token[ii] == '\0')
						{
							shouldIgnore = true;
							break;
						}
						if (!safeIsspace(token[ii]))
						{
							break;
						}
					}
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
						fprintf(stderr,
							"Error: The input CSV data at row %" PRId64 " and column %zu is not a valid number (was '%s').\n",
							rowCount,
							columnCount,
							token);

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
						fprintf(stderr,
							"Error: The input CSV data at row %" PRId64 " and column %zu is not a valid number (was '%s').\n",
							rowCount,
							columnCount,
							token);

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

			token = strtok_r(NULL, ",", &strtokState);
			columnCount++;
		}

		if (columnCount != numberOfDistributions)
		{
			fprintf(stderr,
				"Error: The input CSV data has less than expected entries at data "
				"row %" PRId64 ".\n",
				rowCount);

			return kCommonConstantReturnTypeError;
		}

		rowCount++;
	}

	if (fp != stdin)
	{
		fclose(fp);
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
	const char *		outputFilePath,
	const float *		outputVariables,
	const char * const *	outputVariableNames,
	size_t			numberOfOutputDistributions)
{
	return writeOutputDistributionsToCSV(
						outputFilePath,
						(const void *) outputVariables,
						kFloatingPointVariableTypeFloat,
						outputVariableNames,
						numberOfOutputDistributions);
}

CommonConstantReturnType
writeOutputDoubleDistributionsToCSV(
	const char *		outputFilePath,
	const double *		outputVariables,
	const char * const *	outputVariableNames,
	size_t			numberOfOutputDistributions)
{
	return writeOutputDistributionsToCSV(
						outputFilePath,
						(const void *) outputVariables,
						kFloatingPointVariableTypeDouble,
						outputVariableNames,
						numberOfOutputDistributions);
}

CommonConstantReturnType
writeOutputDistributionsToCSV(
	const char *			outputFilePath,
	const void *			outputVariables,
	FloatingPointVariableType	outputVariablesType,
	const char * const *		outputVariableNames,
	size_t				numberOfOutputDistributions)
{
	FILE *		fp = NULL;
	float *		outputFloatVariables;
	double *	outputDoubleVariables;

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

	if (fp != stdout)
	{
		fclose(fp);
	}

	return kCommonConstantReturnTypeSuccess;
}

void
printJSONVariables(JSONVariable *  jsonVariables, size_t count, const char *  description)
{
	/*
	 *	Print JSON outputs.
	 */
	printf("{\n");
	printf("\t\"description\": \"%s\",\n", description);
	printf("\t\"plots\": [\n");

	for (size_t ii = 0; ii < count; ii++)
	{
		printf("\t\t{\n");

		/*
		 *	We include this property in the JSON for backwards compatibility.
		 */
		printf("\t\t\t\"variableID\": \"%s\",\n", jsonVariables[ii].variableSymbol);
		printf("\t\t\t\"variableSymbol\": \"%s\",\n", jsonVariables[ii].variableSymbol);
		printf("\t\t\t\"variableDescription\": \"%s\",\n", jsonVariables[ii].variableDescription);
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

		printf("\t\t\t\"stdValues\": [\n");
		for (size_t jj = 0; jj < jsonVariables[ii].size; jj++)
		{
			switch (jsonVariables[ii].type)
			{
				case kJSONVariableTypeDouble:
				{
					printf(
						"\t\t\t\t% " SignaloidParticleModifier "f",
						UxHwDoubleNthMoment(jsonVariables[ii].values.asDouble[jj], 2));
					break;
				}
				case kJSONVariableTypeFloat:
				{
					printf(
						"\t\t\t\t% " SignaloidParticleModifier "f",
						UxHwFloatNthMoment(jsonVariables[ii].values.asFloat[jj], 2));
					break;
				}
				case kJSONVariableTypeFloatParticle:
				case kJSONVariableTypeDoubleParticle:
				{
					printf("\t\t\t\t% " SignaloidParticleModifier "f", 0.0);
					break;
				}
				case kJSONVariableTypeUnknown:
				default:
				{
					fatal("kJSONvariableTypeUnknown must be specified");
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
	JSONVariable *	jsonVariable,
	double *	outputVariableValues,
	const char *	outputVariableDescription,
	size_t		outputSelect,
	size_t		numberOfOutputVariableValues)
{
	snprintf(jsonVariable->variableSymbol, kCommonConstantMaxCharsPerJSONVariableSymbol, "outputVariables[%zu]", outputSelect);
	snprintf(jsonVariable->variableDescription, kCommonConstantMaxCharsPerJSONVariableDescription, "%s", outputVariableDescription);
	jsonVariable->values = (JSONVariablePointer){ .asDouble = outputVariableValues };
	jsonVariable->type = kJSONVariableTypeDouble;
	jsonVariable->size = numberOfOutputVariableValues;

	return;
}

void
printJSONFormattedOutput(
	CommonCommandLineArguments *	arguments,
	double *			monteCarloOutputSamples,
	double *			outputVariables,
	const char **			outputVariableDescriptions,
	size_t				numberOfOutputVariables,
	const char *			description)
{
	JSONVariable *	jsonVariables = (JSONVariable *)checkedCalloc(
						numberOfOutputVariables,
						sizeof(JSONVariable),
						__FILE__,
						__LINE__);
	size_t		outputSelectLowerBound;
	size_t		outputSelectUpperBound;

	if (arguments->outputSelect == numberOfOutputVariables)
	{
		outputSelectLowerBound = 0;
		outputSelectUpperBound = numberOfOutputVariables;
	}
	else
	{
		outputSelectLowerBound = arguments->outputSelect;
		outputSelectUpperBound = outputSelectLowerBound + 1;
	}

	for (size_t outputSelect = outputSelectLowerBound; outputSelect < outputSelectUpperBound; outputSelect++)
	{
		/*
		 *	If in Monte Carlo mode, `pointerToOutputVariable` points to the beginning of the `monteCarloOutputSamples` array.
		 *	In this case, `arguments->numberOfMonteCarloIterations` is the length of the `monteCarloOutputSamples` array.
		 *	Else, it points to the entry of the `outputVariables` to be used.
		 *	In this case, `arguments->numberOfMonteCarloIterations` equals 1.
		 */
		double *	pointerToOutputVariable = arguments->isMonteCarloMode ? monteCarloOutputSamples : &outputVariables[outputSelect];

		populateJSONVariableStruct(
			&jsonVariables[outputSelect],
			pointerToOutputVariable,
			outputVariableDescriptions[outputSelect],
			outputSelect,
			arguments->numberOfMonteCarloIterations);
	}

	printJSONVariables(
		&jsonVariables[outputSelectLowerBound],
		outputSelectUpperBound - outputSelectLowerBound,
		description);

	free(jsonVariables);

	return;
}

static void
setDefaultCommandLineArgumentValues(CommonCommandLineArguments *  arguments)
{
	assert(arguments != NULL);

	*arguments = (CommonCommandLineArguments) {
							.outputFilePath			= "",
							.inputFilePath			= "",
							.isWriteToFileEnabled		= false,
							.isTimingEnabled		= false,
							.numberOfMonteCarloIterations	= 1,
							.outputSelect			= 0,
							.isOutputSelected		= false,
							.isVerbose			= false,
							.isInputFromFileEnabled		= false,
							.isOutputJSONMode		= false,
							.isHelpEnabled			= false,
							.isBenchmarkingMode		= false,
							.isMonteCarloMode		= false,
							.isSingleShotExecution		= true,
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
 *	them to convert a single leading '-' into a '+'
 */
static void
modifyNextArgvForNewLibc(char * const  argv[])
{
	if (getOptIsNewlib())
	{
		char *	arg = argv[optind == 0 ? 1 : optind];

		if ((arg != NULL) && (arg[0] == '-') && (arg[1] != '-'))
		{
			arg[0] = '+';
		}
	}

	return;
}

static void
checkDuplicates(
	struct option *		currentOpts,
	size_t			currentOptsSize,
	const char *		newOption)
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

static struct option *
constructlongOptions(
	DemoOption *		demoOpts,
	size_t 			demoOptsSize,
	struct option *		optionsOut,
	size_t			optionsOutSize)
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
		int	hasArg = demoOpts[ii].hasArg ? required_argument : no_argument;
		int	val = ii;

		if ((demoOpts[ii].opt == NULL) && (demoOpts[ii].optAlternative == NULL))
		{
			fatal("Internal Error: Options for demo missing both option names (index %zu).", ii);
		}

		if (demoOpts[ii].opt != NULL)
		{
			checkDuplicates(optionsOut, outIndex, demoOpts[ii].opt);

			optionsOut[outIndex] = (struct option) {
				.name = demoOpts[ii].opt,
				.has_arg = hasArg,
				.flag = NULL,
				.val = val,
			};
			outIndex++;
		}

		if (demoOpts[ii].optAlternative != NULL)
		{
			checkDuplicates(optionsOut, outIndex, demoOpts[ii].optAlternative);

			optionsOut[outIndex] = (struct option) {
				.name = demoOpts[ii].optAlternative,
				.has_arg = hasArg,
				.flag = NULL,
				.val = val,
			};
			outIndex++;
		}
	}

	/*
	 *	Put the final zero entry in
	 */
	optionsOut[outIndex] = (struct option) { ZERO_STRUCT_INIT };
	outIndex++;

	return optionsOut;
}

static CommonConstantReturnType
parseArgsCoreImplementation(
	int		argc,
	char *  const	argv[],
	DemoOption *	options,
	size_t		optionsSize)
{
	size_t		longOptionsSize = 0;
	struct option *	longOptions = NULL;
	bool		error = false;
	int		longIndex;
	int		previousOptInd;
	int		opt;

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
	longOptions = (struct option *)checkedCalloc(longOptionsSize, sizeof(struct option), __FILE__, __LINE__);

	constructlongOptions(options, optionsSize, longOptions, longOptionsSize);

	optind = 0;
	opterr = 0;

	/*
	 *	From the getopt man page:
	 *
	 *	> The variable optind is the index of the next element to be processed in argv.
	 *
	 *	Therefore, we remember the previous value of optind which tells us the index of the
	 *	_current_ index being processed. This is very useful for error messages.
	 */
	previousOptInd = 1;

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
		modifyNextArgvForNewLibc(argv);
		opt = getopt_long_only(argc, argv, ":", longOptions, &longIndex);

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
		assert((size_t)opt < optionsSize);

		if (options[opt].foundOpt != NULL)
		{
			*(options[opt].foundOpt) = true;
		};

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

	free(longOptions);

	return(error ? kCommonConstantReturnTypeError : kCommonConstantReturnTypeSuccess);
}


static CommonConstantReturnType
concatAndParseArgs(
	int		argc,
	char *  const	argv[],
	DemoOption *	demoSpecificOptions,
	DemoOption *	commonOptions)
{
	size_t				demoSpecificOptionsSize = 0;
	size_t				commonOptionsSize = 0;
	size_t				concatOptionsSize;
	DemoOption *			concatOptions = NULL;
	CommonConstantReturnType	ret;

	while ((demoSpecificOptions[demoSpecificOptionsSize].opt != NULL) || (demoSpecificOptions[demoSpecificOptionsSize].optAlternative != NULL))
	{
		demoSpecificOptionsSize++;
	}

	while ((commonOptions[commonOptionsSize].opt != NULL) || (commonOptions[commonOptionsSize].optAlternative != NULL))
	{
		commonOptionsSize++;
	}

	concatOptionsSize = demoSpecificOptionsSize + commonOptionsSize;
	concatOptions = (DemoOption *)checkedCalloc(concatOptionsSize, sizeof(DemoOption), __FILE__, __LINE__);

	memcpy(
		concatOptions,
		demoSpecificOptions,
		sizeof(DemoOption) * demoSpecificOptionsSize);

	memcpy(
		concatOptions + demoSpecificOptionsSize,
		commonOptions,
		sizeof(DemoOption) * commonOptionsSize);

	ret = parseArgsCoreImplementation(argc, argv, concatOptions, concatOptionsSize);

	free(concatOptions);

	return ret;
}

CommonConstantReturnType
parseArgs(
	int				argc,
	char *  const 			argv[],
	CommonCommandLineArguments *	arguments,
	DemoOption *			demoSpecificOptions)
{
	assert(arguments != NULL);

	setDefaultCommandLineArgumentValues(arguments);

	const char *	inputArg = NULL;
	const char *	outputArg = NULL;
	const char *	outputSelectArg = NULL;
	const char *	multipleExecutionsArg = NULL;
	DemoOption	commonOptions[] = {
						{ "input",			"i",	true,	&inputArg,		NULL },
						{ "output",			"o",	true,	&outputArg,		NULL },
						{ "select-output",		"S",	true,	&outputSelectArg,	NULL },
						{ "time",			"T",	false,	NULL,			&arguments->isTimingEnabled },
						{ "multiple-executions",	"M",	true,	&multipleExecutionsArg,	NULL },
						{ "verbose",			"v",	false,	NULL,			&arguments->isVerbose },
						{ "json",			"j",	false,	NULL,			&arguments->isOutputJSONMode },
						{ "help",			"h",	false,	NULL,			&arguments->isHelpEnabled },
						{ "benchmarking",		"b",	false,	NULL,			&arguments->isBenchmarkingMode },
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
		int	outputSelect;
		int	ret = parseIntChecked(outputSelectArg, &outputSelect);

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
			arguments->outputSelect = outputSelect;
			arguments->isOutputSelected = true;
		}
	}

	if (multipleExecutionsArg != NULL)
	{
		int	multipleExecutions;
		int	ret = parseIntChecked(multipleExecutionsArg, &multipleExecutions);

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

			arguments->isMonteCarloMode = true;
			arguments->isTimingEnabled = true;
			arguments->isSingleShotExecution = false;
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
		"\t[-S, --select-output <output : int>] (Compute 0-indexed output, by default 0.)\n"
		"\t[-M, --multiple-executions <Number of executions : int (Default: 1)>] (Repeated execute kernel for benchmarking.)\n"
		"\t[-T, --time] (Timing mode: Times and prints the timing of the kernel execution.)\n"
		"\t[-v, --verbose] (Verbose mode: Prints extra information about demo execution.)\n"
		"\t[-b, --benchmarking] (Benchmarking mode: Generate outputs in format for benchmarking.)\n"
		"\t[-j, --json] (Print output in JSON format.)\n"
		"\t[-h, --help] (Display this help message.)\n");

	return;
}

MeanAndVariance
calculateMeanAndVarianceOfFloatSamples(
	const float *	dataArray,
	size_t		dataArraySize)
{
	float	mean;
	float	variance;
	float	sum = 0;
	float	sumOfSquares = 0;

	for (size_t ii = 0; ii < dataArraySize; ii++)
	{
		sum += dataArray[ii];
		sumOfSquares += dataArray[ii] * dataArray[ii];
	}

	mean = sum / dataArraySize;
	variance = sumOfSquares / dataArraySize - (mean * mean);

	return (MeanAndVariance)
	{
		.mean = (double)mean,
		.variance = (double)variance,
	};
}

MeanAndVariance
calculateMeanAndVarianceOfDoubleSamples(
	const double *	dataArray,
	size_t		dataArraySize)
{
	double	mean;
	double	variance;
	double	sum = 0;
	double	sumOfSquares = 0;

	for (size_t ii = 0; ii < dataArraySize; ii++)
	{
		sum += dataArray[ii];
		sumOfSquares += dataArray[ii] * dataArray[ii];
	}

	mean = sum / dataArraySize;
	variance = sumOfSquares / dataArraySize - (mean * mean);

	return (MeanAndVariance)
	{
		.mean = mean,
		.variance = variance,
	};
}

static int
compareFloats(const void *a, const void *b)
{
	float diff = *(const float*)a - *(const float*)b;

	return ((int) (diff > 0)) - ((int) (diff < 0));
}

static int
compareDoubles(const void *a, const void *b)
{
	double diff = *(const double*)a - *(const double*)b;

	return ((int) (diff > 0)) - ((int) (diff < 0));
}

float
calculatePercentageQuantileOfFloatSamples(
	const float *	dataArray,
	float		quantilePercentage,
	size_t		dataArraySize)
{
	int index;
	float quantile;

	/*
	 *	Make a copy for sorting.
	 */ 
	float *	dataArrayCopy = (float *)checkedMalloc(
				dataArraySize * sizeof(float),
				__FILE__,
				__LINE__);

	memcpy(dataArrayCopy, dataArray, dataArraySize * sizeof(float));

	/*
	 *	Sort samples (modifies original array).
	 */ 
	qsort(dataArrayCopy, dataArraySize, sizeof(float), compareFloats);

	/*
	 *	Determine the relevant index.
	 */
	index = (int)(quantilePercentage * dataArraySize);

	/*
	 *	Return the value at that index.
	 */
	quantile = dataArrayCopy[index];
	free(dataArrayCopy);

	return quantile;
}

double
calculatePercentageQuantileOfDoubleSamples(
	const double *	dataArray,
	double		quantilePercentage,
	size_t		dataArraySize)
{
	int 	index;
	double 	quantile;

	/*
	 *	Make a copy for sorting.
	 */ 
	double * dataArrayCopy = (double *)checkedMalloc(
			dataArraySize * sizeof(double),
			__FILE__,
			__LINE__);

	memcpy(dataArrayCopy, dataArray, dataArraySize * sizeof(double));

	/*
	 *	Sort samples (modifies original array).
	 */ 
	qsort(dataArrayCopy, dataArraySize, sizeof(double), compareDoubles);

	/*
	 *	Determine the relevant index.
	 */
	index = (int)(quantilePercentage * dataArraySize);

	/*
	 *	Return the value at that index.
	 */
	quantile = dataArrayCopy[index];
	free(dataArrayCopy);

	return quantile;
}

void
calculateMeanAndVarianceOfMultiDimensionalFloatSamples(
	float **	dataArray,
	size_t		dataArrayRows,
	size_t		dataArrayColumns,
	float *		meanValueArray,
	float *		varianceArray)
{
	for (size_t jj = 0; jj < dataArrayColumns; jj++)
	{
		float		sum = 0;
		float		sumOfSquares = 0;
		float 		meanValue;

		for (size_t ii = 0; ii < dataArrayRows; ii++)
		{
			sum += dataArray[ii][jj];
			sumOfSquares += dataArray[ii][jj] * dataArray[ii][jj];
		}

		meanValue = sum / dataArrayRows;
		meanValueArray[jj] = meanValue;
		varianceArray[jj] = sumOfSquares / dataArrayRows - (meanValue * meanValue);
	}

	return;
}

void
calculateMeanAndVarianceOfMultiDimensionalDoubleSamples(
	double **	dataArray,
	size_t		dataArrayRows,
	size_t		dataArrayColumns,
	double *	meanValueArray,
	double *	varianceArray)
{
	for (size_t jj = 0; jj < dataArrayColumns; jj++)
	{
		double		sum = 0;
		double		sumOfSquares = 0;

		for (size_t ii = 0; ii < dataArrayRows; ii++)
		{
			sum += dataArray[ii][jj];
			sumOfSquares += dataArray[ii][jj] * dataArray[ii][jj];
		}

		meanValueArray[jj] = sum / dataArrayRows;
		varianceArray[jj] = sumOfSquares / dataArrayRows - (meanValueArray[jj] * meanValueArray[jj]);
	}

	return;
}



void
saveMonteCarloFloatDataToDataDotOutFile(
	const float *	benchmarkingDataSamples,
	uint64_t	cpuTimeElapsedMicroSeconds,
	size_t		numberOfMonteCarloIterations)
{
	FILE *	fp = fopen("data.out", "w");
	if (fp == NULL)
	{
		fatal("Could not open monte carlo output file");
	}

	fprintf(fp, "%" PRIu64 "\n", cpuTimeElapsedMicroSeconds);

	for (size_t ii = 0; ii < numberOfMonteCarloIterations; ii++)
	{
		fprintf(fp, "%.20f\n", benchmarkingDataSamples[ii]);
	}

	fclose(fp);

	return;
}

void
saveMonteCarloFloatMultidimensionalDataToDataDotOutFile(
	float **	benchmarkingDataSamples,
	uint64_t	cpuTimeElapsedMicroSeconds,
	size_t		numberOfOutputVariables,
	size_t		numberOfMonteCarloIterations)
{
	FILE *	fp = fopen("data.out", "w");
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

	fclose(fp);

	return;
}

void
saveMonteCarloDoubleDataToDataDotOutFile(
	const double *	benchmarkingDataSamples,
	uint64_t	cpuTimeElapsedMicroSeconds,
	size_t		numberOfMonteCarloIterations)
{
	FILE *	fp = fopen("data.out", "w");
	if (fp == NULL)
	{
		fatal("Could not open monte carlo output file");
	}

	fprintf(fp, "%" PRIu64 "\n", cpuTimeElapsedMicroSeconds);

	for (size_t ii = 0; ii < numberOfMonteCarloIterations; ii++)
	{
		fprintf(fp, "%.20lf\n", benchmarkingDataSamples[ii]);
	}

	fclose(fp);

	return;
}

void
saveMonteCarloDoubleMultidimensionalDataToDataDotOutFile(
	double **	benchmarkingDataSamples,
	uint64_t	cpuTimeElapsedMicroSeconds,
	size_t		numberOfOutputVariables,
	size_t		numberOfMonteCarloIterations)
{
	FILE *	fp = fopen("data.out", "w");
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

	fclose(fp);

	return;
}

void *
checkedMalloc(size_t size, const char *  file, int line)
{
	void *	ret = malloc(size);
	if (ret == NULL)
	{
		fatal("malloc() failed to allocate %zu bytes at %s:%d", size, file, line);
	}

	return ret;
}

void *
checkedCalloc(size_t count, size_t size, const char *  file, int line)
{
	void *	ret = calloc(count, size);
	if (ret == NULL)
	{
		fatal("calloc() failed to allocate %zu bytes at %s:%d", count * size, file, line);
	}

	return ret;
}
