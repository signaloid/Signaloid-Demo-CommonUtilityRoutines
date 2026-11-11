# Common Utilities Library Documentation

## Summary

This is a C utility library from Signaloid that provides common functionality for command-line applications that perform deterministic computation on probability distributions. It also supports Monte Carlo mode for benchmarking. The library handles CSV I/O, command-line argument parsing, JSON output formatting, and statistical calculations.

## Quick Start Guide
This library parses command line arguments using the `CommonCommandLineArguments` and the `DemoOption` structs.
`CommonCommandLineArguments` supports the following options
- `-i, --input <file>` - Read inputs from CSV file
- `-o, --output <file>` - Write outputs to CSV file
- `-S, --select-output <n>` - Compute specific output (0-indexed). Set `n` to the number of outputs to compute all outputs
- `-M, --multiple-executions <n>` - Run multiple iterations for benchmarking. Also enables timing
- `-T, --time` - Enable timing measurements
- `-v, --verbose` - Enable verbose output
- `-b, --benchmarking` - Benchmarking output format
- `-j, --json` - JSON output format
- `-h, --help` - Display help message

Option names must be given in full. Abbreviations such as `-m` for `-M` are errors. `-j` and `-b` cannot be used together.

To provide demo specific options specify the `DemoOption` structs:
```c
typedef struct {
        const char *    opt;                    /* Option name */
        const char *    optAlternative;         /* Alternative option name, or NULL */
        bool            hasArg;                 /* Requires argument? */
        const char **   foundArg;               /* Where to store argument */
        bool *          foundOpt;               /* Flag set if option found  */
} DemoOption;
```

- `DemoOption.opt` The command-line flag (without leading `-` or `--`) for this option.
- `DemoOption.optAlternative` An alternative command-line flag (without leading `-` or `--`) for this option. Often used for short options.
- `DemoOption.hasArg`  Whether this option takes an argument. If set, then parsing will fail unless the command-line flag has an argument.
- `DemoOption.foundArg` Pass pointer to variable in which any argument will be stored. Set to `NULL` if this option does not have an argument. `parseArgs` sets it to `NULL` before parsing, so it cannot hold a default value.
- `DemoOption.foundOpt` Pass pointer to variable which will be set to true if option is found. May be set to `NULL`, in which case nothing will be written. `parseArgs` sets it to `false` before parsing.

Example `DemoOption` usage:
```c
        DemoOption myOptions[] = {
                { "custom-flag", "c", true, &customArg, &foundCustom },
                { ZERO_STRUCT_INIT } /* Terminator */
        };
```

## Command-Line Parsing
To parse command-line arguments use the method

```c
CommonConstantReturnType
parseArgs(
        int                             argc,
        char *  const                   argv[],
        CommonCommandLineArguments *    arguments,
        DemoOption *                    demoSpecificOptions
);
```

Parses both common and application-specific command-line options. Pass `NULL` for `demoSpecificOptions` if the demo has no extra options.

```c
void
printCommonUsage(void);
```

Prints the usage of the common options to `stderr`.

### Minimal working example
```c
#include "common.h"

int
main(int argc, char *  argv[])
{
        CommonCommandLineArguments      args;
        const char *                    customArg       = NULL;
        bool                            foundCustom     = false;

        /*
         *      Define demo specific options
         */
        DemoOption myOptions[] = {
                { "custom-flag", "c", true, &customArg, &foundCustom },
                { ZERO_STRUCT_INIT }  /* Terminator */
        };

        /*
         *      Parse arguments
         */
        if (parseArgs(argc, argv, &args, myOptions) != kCommonConstantReturnTypeSuccess)
        {
                printCommonUsage();

                return 1;
        }

        return 0;
}
```



## API Documentation

### CSV Input/Output

#### Reading Data

```c
CommonConstantReturnType
readInputFloatDistributionsFromCSV(
        const char *            inputFilePath,
        const char *  const *   expectedHeaders,
        float *                 inputDistributions,
        size_t                  numberOfDistributions
);

CommonConstantReturnType
readInputDoubleDistributionsFromCSV(
        const char *            inputFilePath,
        const char *  const *   expectedHeaders,
        double *                inputDistributions,
        size_t                  numberOfDistributions
);
```

Reads CSV files containing either regular numeric values or Ux-valued distributions. Automatically detects Ux values and converts sample data to distributions.

The CSV file must have this layout.
- The first row holds the headers in `expectedHeaders`, in the same order. Matching is case-sensitive. Spaces around a header are ignored.
- Each column becomes one distribution. In a column of numbers, the numbers are the samples of the distribution.
- A Ux value is only allowed in the first data row. All later rows of that column must be `-`.
- A `-` entry is a missing value and is skipped.
- There can be at most `kCommonConstantMaxNumberOfInputSamples` (10000) data rows, and each column needs at least one value.
- Blank lines and a UTF-8 byte order mark at the start of the file are skipped.

`inputFilePath` must be a file. Reading from `"stdin"` is not supported, so demos need the `-i` option to read a CSV file.

**Returns:** `kCommonConstantReturnTypeSuccess` or `kCommonConstantReturnTypeError`

#### Writing Data

```c
CommonConstantReturnType
writeOutputFloatDistributionsToCSV(
        const char *            outputFilePath,
        const float *           outputVariables,
        const char *  const *   outputVariableNames,
        size_t  numberOfOutputDistributions
);

CommonConstantReturnType
writeOutputDoubleDistributionsToCSV(
        const char *            outputFilePath,
        const double *          outputVariables,
        const char *  const *   outputVariableNames,
        size_t                  numberOfOutputDistributions
);
```

Writes output variables to CSV format with headers. The file has one header row with the names in `outputVariableNames`, then one row of values. Entries are separated by `, ` and values are printed with `%e`. Use `"stdout"` as `outputFilePath` to write to stdout. Returns `kCommonConstantReturnTypeError` if the file cannot be opened or writing fails.


### String Parsing

```c
CommonConstantReturnType parseIntChecked(const char *  str, int *  out);
CommonConstantReturnType parseFloatChecked(const char *  str, float *  out);
CommonConstantReturnType parseDoubleChecked(const char *  str, double *  out);
```

Safe parsing functions that validate input and handle errors gracefully. They parse a number at the start of `str` and ignore any trailing characters.

### JSON Output

```c
void
printJSONVariables(
        JSONVariable *  jsonVariables,
        size_t          count,
        const char *    description
);
```

Outputs variables in structured JSON format for integration with visualization tools.

```c
void
populateJSONVariableStruct(
        JSONVariable *  jsonVariable,
        double *        outputVariableValues,
        const char *    outputVariableDescription,
        size_t          outputSelect,
        size_t          numberOfOutputVariableValues
);
```

Populates a `JSONVariable` struct for use with `printJSONVariables`. Sets the variable symbol to `outputVariables[N]` where N is the outputSelect index.

```c
void
printJSONFormattedOutput(
        CommonCommandLineArguments *        arguments,
        double *                            monteCarloOutputSamples,
        double *                            outputVariables,
        const char **                       outputVariableDescriptions,
        size_t                              numberOfOutputVariables,
        const char *                        description,
        const kOutputVariableTypeIndex *    outputVariableTypes
);
```

Prints output distributions in JSON format. If `arguments->outputSelect` equals `numberOfOutputVariables`, prints all outputs; otherwise prints only the selected output. In Monte Carlo mode, uses `monteCarloOutputSamples`; otherwise uses entries from `outputVariables`. Outputs marked `kOutputVariableTypeScalar` in `outputVariableTypes` always use their single entry from `outputVariables`. Pass `NULL` for `outputVariableTypes` if all outputs are distributions. In Monte Carlo mode, `monteCarloOutputSamples` holds one output, so it calls `fatal()` if all outputs are selected and more than one is a distribution.

```c
bool
isSelectedOutputScalar(
        const CommonCommandLineArguments *  arguments,
        const kOutputVariableTypeIndex *    outputVariableTypes,
        size_t                              numberOfOutputVariables
);
```

Returns `true` if a single output is selected and its type is `kOutputVariableTypeScalar`. Useful in Monte Carlo mode, where a scalar output has one value rather than `numberOfMonteCarloIterations` samples.

### Human-Readable Output

```c
void
printHumanConsumableOutput(
        CommonCommandLineArguments *        arguments,
        size_t                              numberOfOutputVariables,
        double *                            outputVariables,
        const char *                        outputVariableNames[],
        const char *                        outputVariableDescriptions[],
        double *                            monteCarloOutputSamples,
        const kOutputVariableTypeIndex *    outputVariableTypes
);
```

Prints the selected outputs in a human-readable format. Output selection, Monte Carlo mode and `outputVariableTypes` work as in `printJSONFormattedOutput`.

```c
void
determineIndexRangeOfSelectedOutputs(
        CommonCommandLineArguments *    arguments,
        size_t                          numberOfOutputVariables,
        size_t *                        pointerToOutputSelectLowerBound,
        size_t *                        pointerToOutputSelectUpperBound
);
```

Sets the range `[lower, upper)` of the outputs selected with `-S`. If `arguments->outputSelect` equals `numberOfOutputVariables`, the range covers all outputs. Calls `fatal()` if `arguments->outputSelect` is greater than `numberOfOutputVariables`.

### Statistical Functions

```c
MeanAndVariance
calculateMeanAndVarianceOfFloatSamples(
        const float *   dataArray,
        size_t          dataArraySize
);

MeanAndVariance
calculateMeanAndVarianceOfDoubleSamples(
        const double *  dataArray,
        size_t          dataArraySize
);
```

Calculates the mean and the population variance from sample arrays. Calls `fatal()` if `dataArraySize` is 0.

```c
float
calculatePercentageQuantileOfFloatSamples(
        const float *   dataArray,
        float           quantilePercentage,
        size_t          dataArraySize
);

double
calculatePercentageQuantileOfDoubleSamples(
        const double *  dataArray,
        double          quantilePercentage,
        size_t          dataArraySize
);
```

Calculates the specified quantile (e.g., 0.95 for 95th percentile) from sample arrays. It sorts a copy of the samples and returns the sample at index `floor(quantilePercentage * dataArraySize)`, without interpolation. A `quantilePercentage` of 1 returns the largest sample. Calls `fatal()` if `dataArraySize` is 0, `quantilePercentage` is outside [0, 1], or a sample is NaN.

```c
void
calculateMeanAndVarianceOfMultiDimensionalFloatSamples(
        float **        dataArray,
        size_t          dataArrayRows,
        size_t          dataArrayColumns,
        float *         meanValueArray,
        float *         varianceArray
);

void
calculateMeanAndVarianceOfMultiDimensionalDoubleSamples(
        double **       dataArray,
        size_t          dataArrayRows,
        size_t          dataArrayColumns,
        double *        meanValueArray,
        double *        varianceArray
);
```

Calculates the mean and population variance for each column in a 2D array of samples (e.g., multiple output variables from Monte Carlo iterations). `dataArray[row][column]` holds sample `row` of variable `column`. Calls `fatal()` if `dataArrayRows` is 0.

### Monte Carlo Support

```c
CommonConstantReturnType
saveMonteCarloFloatDataToDataDotOutFile(
        const float *   benchmarkingDataSamples,
        uint64_t        cpuTimeElapsedMicroSeconds,
        size_t          numberOfMonteCarloIterations
);

CommonConstantReturnType
saveMonteCarloDoubleDataToDataDotOutFile(
        const double *  benchmarkingDataSamples,
        uint64_t        cpuTimeElapsedMicroSeconds,
        size_t          numberOfMonteCarloIterations
);
```

Saves Monte Carlo simulation results for a single output variable to `data.out` file. The first line is `cpuTimeElapsedMicroSeconds`, then one sample per line. Calls `fatal()` if the file cannot be opened. Returns `kCommonConstantReturnTypeError` if writing fails.

```c
CommonConstantReturnType
saveMonteCarloFloatMultidimensionalDataToDataDotOutFile(
        float **        benchmarkingDataSamples,
        uint64_t        cpuTimeElapsedMicroSeconds,
        size_t          numberOfOutputVariables,
        size_t          numberOfMonteCarloIterations
);

CommonConstantReturnType
saveMonteCarloDoubleMultidimensionalDataToDataDotOutFile(
        double **       benchmarkingDataSamples,
        uint64_t        cpuTimeElapsedMicroSeconds,
        size_t          numberOfOutputVariables,
        size_t          numberOfMonteCarloIterations
);
```

Saves Monte Carlo simulation results for multiple output variables to `data.out` file. The first line is `cpuTimeElapsedMicroSeconds`. Each following line contains comma-separated samples for all variables. `benchmarkingDataSamples[variable][iteration]` holds one sample. This is the opposite layout to the multidimensional mean and variance functions. Calls `fatal()` if the file cannot be opened. Returns `kCommonConstantReturnTypeError` if writing fails.

### Utility Functions

```c
void doNotOptimize(void *  ptr);  /* Prevents compiler optimization */
void fatal(const char *  fmt, ...);  /* Prints error and exits */
void *  checkedMalloc(size_t size, const char *  file, int line);
void *  checkedCalloc(size_t count, size_t size, const char *  file, int line);
```

## Types

```c
typedef enum
{
        kOutputVariableTypeDistribution = 0,
        kOutputVariableTypeScalar       = 1,
} kOutputVariableTypeIndex;
```

Marks each output as a distribution or a scalar. Used by `printJSONFormattedOutput`, `printHumanConsumableOutput` and `isSelectedOutputScalar`.

```c
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
        const double *  asDouble;
        const float *   asFloat;
} JSONVariablePointer;

typedef struct JSONVariable
{
        char                variableSymbol[kCommonConstantMaxCharsPerJSONVariableSymbol];
        char                variableDescription[kCommonConstantMaxCharsPerJSONVariableDescription];
        JSONVariablePointer values;
        JSONVariableType    type;
        size_t              size;
} JSONVariable;
```

A `JSONVariable` holds `size` values for `printJSONVariables`. Set `type` to match the member of `values` that is used. The `Float` and `Double` types are distributions. Their mean and standard deviation are taken from the distribution. The `Particle` types are plain values. Their mean is the value itself and their standard deviation is 0. `kJSONVariableTypeUnknown` causes a `fatal()` error.

```c
typedef struct
{
        double  mean;
        double  variance;
} MeanAndVariance;
```

Returned by `calculateMeanAndVarianceOfFloatSamples` and `calculateMeanAndVarianceOfDoubleSamples`.

## Constants

```c
kCommonConstantMaxCharsPerFilepath = 1024
kCommonConstantMaxCharsPerLine = 1048576
kCommonConstantMaxNumberOfInputSamples = 10000
kCommonConstantMaxCharsPerJSONVariableSymbol = 256
kCommonConstantMaxCharsPerJSONVariableDescription = 1024
```

## Error Handling

Functions that can fail return `CommonConstantReturnType`:
- `kCommonConstantReturnTypeSuccess` (0) - Operation succeeded
- `kCommonConstantReturnTypeError` (1) - Operation failed

The library uses `fatal()` for unrecoverable errors and prints descriptive error messages to `stderr` for recoverable issues.

## Example usage

### JSON printing usage
```c
#include <stdio.h>
#include <string.h>
#include <uxhw.h>
#include "common.h"

int
main(void)
{
        JSONVariable    jsonVariable;
        double          gaussOutput = UxHwDoubleGaussDist(0.0, 1.0);

        snprintf(
                jsonVariable.variableSymbol,
                kCommonConstantMaxCharsPerJSONVariableSymbol,
                "%s",
                "gaussOutput"
        );
        snprintf(
                jsonVariable.variableDescription,
                kCommonConstantMaxCharsPerJSONVariableDescription,
                "%s",
                "A Gaussian distribution"
        );
        jsonVariable.values.asDouble    = &gaussOutput;
        jsonVariable.type               = kJSONVariableTypeDouble;
        jsonVariable.size               = 1;

        printJSONVariables(&jsonVariable, 1, "My output");

        return 0;
}
```
On Signaloid hardware this outputs something like
```json
{
        "description": "My output",
        "plots": [
                {
                        "variableID": "gaussOutput",
                        "variableSymbol": "gaussOutput",
                        "variableDescription": "A Gaussian distribution",
                        "values": [
                                "0.000000"
                        ],
                        "particleValues": [
                                 0.000000
                        ],
                        "meanValues": [
                                 0.000000
                        ],
                        "stdDevValues": [
                                 0.999500
                        ]
                }
        ]
}
```


### CSV reading/writing usage

Run this example with `-i input.csv -o output.csv`. Without `-i` and `-o`, reading and writing fail.

```c
#include "common.h"

int
main(int argc, char *  argv[])
{
        CommonCommandLineArguments      args;

        /*
         *      Parse arguments. This demo has no extra options.
         */
        if (parseArgs(argc, argv, &args, NULL) != kCommonConstantReturnTypeSuccess)
        {
                printCommonUsage();

                return 1;
        }

        /*
         *      Read input from CSV
         */
        float           inputs[3];
        const char *    headers[] = {"value1", "value2", "value3"};

        if (readInputFloatDistributionsFromCSV(args.inputFilePath, headers, inputs, 3) != kCommonConstantReturnTypeSuccess)
        {
                return 1;
        }

        /*
         *      Process data
         */
        float           outputs[2] = {inputs[0] + inputs[1], inputs[1] * inputs[2]};
        const char *    outputNames[] = {"output1", "output2"};

        /*
         *      Write output to CSV
         */
        if (writeOutputFloatDistributionsToCSV(args.outputFilePath, outputs, outputNames, 2) != kCommonConstantReturnTypeSuccess)
        {
                return 1;
        }

        return 0;
}
```



## License

MIT License - Copyright (c) 2023-2026, Signaloid
