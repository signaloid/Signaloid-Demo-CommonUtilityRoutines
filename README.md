# Common Utilities Library Documentation

## Summary

This is a C utility library from Signaloid that provides common functionality for command-line applications that perform deterministic computation on probability distributions. It also supports Monte Carlo mode for benchmarking. The library handles CSV I/O, command-line argument parsing, JSON output formatting, and statistical calculations.

## Quick Start Guide
This library parses command line arguments using the `CommonCommandLineArguments` and the `DemoOption` structs.
`CommonCommandLineArguments` supports the following options
- `-i, --input <file>` - Read inputs from CSV file
- `-o, --output <file>` - Write outputs to CSV file
- `-S, --select-output <n>` - Compute specific output (0-indexed)
- `-M, --multiple-executions <n>` - Run multiple iterations for benchmarking
- `-T, --time` - Enable timing measurements
- `-v, --verbose` - Enable verbose output
- `-b, --benchmarking` - Benchmarking output format
- `-j, --json` - JSON output format
- `-h, --help` - Display help message

To provide demo specific options specify the `DemoOption` structs:
```c
typedef struct {
        const char *    opt;                    /* Long option name */
        const char *    optAlternative;         /* Short option name */
        bool            hasArg;                 /* Requires argument? */
        const char **   foundArg;               /* Where to store argument */
        bool *          foundOpt;               /* Flag set if option found  */
} DemoOption;
```

- `DemoOption.opt` The command-line flag (without leading `-` or `--`) for this option.
- `DemoOption.optAlternative` An alternative command-line flag (without leading `-` or `--`) for this option. Often used for short options.
- `DemoOption.hasArg`  Whether this option takes an argument. If set, then parsing will fail unless the command-line flag has an argument.
- `DemoOption.foundArg` Pass pointer to variable in which any argument will be stored. Set to `NULL` if this option does not have an argument.
- `DemoOption.foundOpt` Pass pointer to variable which will be set to true if option is found. May be set to `NULL`, in which case nothing will be written.

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

Parses both common and application-specific command-line options.

### Minimal working example
```c
#include "common.h"

int main (int argc, char *  argv[])
{
        CommonCommandLineArguments args;

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

Writes output variables to CSV format with headers.


### String Parsing

```c
CommonConstantReturnType parseIntChecked(const char *  str, int *  out);
CommonConstantReturnType parseFloatChecked(const char *  str, float *  out);
CommonConstantReturnType parseDoubleChecked(const char *  str, double *  out);
```

Safe parsing functions that validate input and handle errors gracefully.

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
        CommonCommandLineArguments *    arguments,
        double *                        monteCarloOutputSamples,
        double *                        outputVariables,
        const char **                   outputVariableDescriptions,
        size_t                          numberOfOutputVariables,
        const char *                    description
);
```

Prints output distributions in JSON format. If `arguments->outputSelect` equals `numberOfOutputVariables`, prints all outputs; otherwise prints only the selected output. In Monte Carlo mode, uses `monteCarloOutputSamples`; otherwise uses entries from `outputVariables`.

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

Calculates mean and variance from sample arrays.

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

Calculates the specified quantile (e.g., 0.95 for 95th percentile) from sample arrays.

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

Calculates mean and variance for each column in a 2D array of samples (e.g., multiple output variables from Monte Carlo iterations).

### Monte Carlo Support

```c
void
saveMonteCarloFloatDataToDataDotOutFile(
        const float *   benchmarkingDataSamples,
        uint64_t        cpuTimeElapsedMicroSeconds,
        size_t          numberOfMonteCarloIterations
);

void
saveMonteCarloDoubleDataToDataDotOutFile(
        const double *  benchmarkingDataSamples,
        uint64_t        cpuTimeElapsedMicroSeconds,
        size_t          numberOfMonteCarloIterations
);
```

Saves Monte Carlo simulation results for a single output variable to `data.out` file.

```c
void
saveMonteCarloFloatMultidimensionalDataToDataDotOutFile(
        float **        benchmarkingDataSamples,
        uint64_t        cpuTimeElapsedMicroSeconds,
        size_t          numberOfOutputVariables,
        size_t          numberOfMonteCarloIterations
);

void
saveMonteCarloDoubleMultidimensionalDataToDataDotOutFile(
        double **       benchmarkingDataSamples,
        uint64_t        cpuTimeElapsedMicroSeconds,
        size_t          numberOfOutputVariables,
        size_t          numberOfMonteCarloIterations
);
```

Saves Monte Carlo simulation results for multiple output variables to `data.out` file. Each line contains comma-separated samples for all variables.

### Utility Functions

```c
void doNotOptimize(void *  ptr);  /* Prevents compiler optimization */
void fatal(const char *  fmt, ...);  /* Prints error and exits */
void *  checkedMalloc(size_t size, const char *  file, int line);
void *  checkedCalloc(size_t count, size_t size, const char *  file, int line);
```

## Constants

```c
kCommonConstantMaxCharsPerFilepath = 1024
kCommonConstantMaxCharsPerLine = 1048576
kCommonConstantMaxNumberOfInputSamples = 10000
```

## Error Handling

All functions return `CommonConstantReturnType`:
- `kCommonConstantReturnTypeSuccess` (0) - Operation succeeded
- `kCommonConstantReturnTypeError` (1) - Operation failed

The library uses `fatal()` for unrecoverable errors and prints descriptive error messages to `stderr` for recoverable issues.
# Examples usage

## JSON printing usage
```c
#include "common.h"
/*
 *   Program constants
 */
typedef enum
{
        kDefaultJSONSize = 1,
};

static const double kDefaultMean        = 0.0;
static const double kDefaultVariance    = 1.0;
int main(int argc, char *  argv[])
{
        JSONVariable jsonVariable;
        double gaussOutput = UxHwDoubleGaussDist(kDefaultMean, kDefaultVariance);
        jsonVariable.values.asDouble = &gaussOutput;

        strncpy(
                jsonVariable.variableSymbol,
                "gaussOutput",
                kCommonConstantMaxCharsPerJSONVariableSymbol
        );
        strncpy(
                jsonVariable.variableDescription,
                "A scaled gaussian distribution",
                kCommonConstantMaxCharsPerJSONVariableDescription
        );

        jsonVariable.type = kJSONVariableTypeDouble;
        jsonVariable.size = kDefaultJSONSize;

        printJSONVariables(
                &jsonVariable,
                kDefaultJSONSize,
                "My output"
        );

        return 0;
}
```
This outputs
```bash
{
        "description": "My output",
        "plots": [
                {
                        "variableID": "gaussOutput",
                        "variableSymbol": "gaussOutput",
                        "variableDescription": "A scaled gaussian distribution",
                        "values": [
                                "0.000000"
                        ],
                        "stdValues": [
                                0.999500
                        ]
                }
        ]
}
```


## CSV reading/writing usage

```c
#include "common.h"

int main(int argc, char *  argv[]) {
        CommonCommandLineArguments args;

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
        if (parseArgs(argc, argv, &args, myOptions) != kCommonConstantReturnTypeSuccess) {
                printCommonUsage();

                return 1;
        }

        /* 
         *      Read input from CSV
         */
        float inputs[3];
        const char *  headers[] = {"value1", "value2", "value3"};
        readInputFloatDistributionsFromCSV(args.inputFilePath, headers, inputs, 3);

        /*
         *      Process data...
         */

        /*
         *      Write output to CSV
         */
        float outputs[2] = {result1, result2};
        const char *  outputNames[] = {"output1", "output2"};
        writeOutputFloatDistributionsToCSV(args.outputFilePath, outputs, outputNames, 2);

        return 0;
}
```



## License

MIT License - Copyright (c) 2023-2025, Signaloid
