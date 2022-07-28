#include <onnxruntime_cxx_api.h>

using namespace std;

#include <chrono>
#include <cmath>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

#include <string>

// ROOT
#include "TFile.h"
#include "TTree.h"

#include "lyra.hpp"
#include "onnxutil.h"
#include "onnxlib.h"

using namespace std::chrono;



int main(int argc, char* argv[]) {

    bool help               = false;
    bool verbose            = false;

    int N                   = 0;
    int channel             = 27;

    std::string modelfile   = "tfmodel.onnx";
    std::string rootfile    = "rootfile.root";

    auto cli = lyra::cli()
        | lyra::opt(verbose)
            ["-v"]["--verbose"]
            ("Verbose mode")
        | lyra::opt(modelfile, "model" )
            ["-m"]["--model"]
            ("File containing the ONNX model")
        | lyra::opt(rootfile, "root" )
            ["-r"]["--root"]
            ("ROOT file to be read")
        | lyra::opt(N, "N")
            ["-N"]["--Nentries"]
            ("Number of entries to process")                        
        | lyra::opt(channel, "channel")
            ["-c"]["--channel"]
            ("Caloriemter channel")
        | lyra::help(help);

    auto result = cli.parse( { argc, argv } );
    if( !result ) {std::cerr << "Error in command line: " << result.errorMessage() << std::endl;exit(1);}
    if(help) {std::cout << cli << std::endl; exit(0);}

    if(verbose) {
        std::cout << "*** Verbose mode selected" << std::endl << "*** Model file: " << modelfile << std::endl;
        std::cout << "*** ROOT file: " << rootfile << std::endl;
    }
 
    std::string instanceName{"fit"};

    OnnxSession* oS = new OnnxSession(modelfile.c_str(), instanceName.c_str());

    size_t numInputNodes = oS->_numInputNodes, numOutputNodes = oS->_numOutputNodes;
    std::vector<int64_t> inputDims = oS->_inputDimensions, outputDims = oS->_outputDimensions;
    const char* inputName = oS->_inputName; const char* outputName = oS->_outputName;
    ONNXTensorElementDataType inputType = oS->_inputType, outputType = oS->_outputType;

    if(verbose) {
        std::cout << "*** Input Nodes:\t"   << numInputNodes << ",\t\t Output Nodes: "  << numOutputNodes << std::endl;
        std::cout << "*** Input Name:\t\t"  << inputName     << ",\t Input Type: "      << inputType  << ",\t Input Dimensions:\t"  << inputDims    << std::endl;
        std::cout << "*** Output Name:\t"   << outputName    << ",\t Output Type: "     << outputType << ",\t Output Dimensions:\t" << outputDims   << std::endl;
    }


    size_t inputTensorSize = vectorProduct(inputDims), outputTensorSize = vectorProduct(outputDims);

    // Proceed to read the data from a ROOT tree:
    TFile f(TString(rootfile.c_str()));
    if (f.IsZombie()) { cout << "Error opening file" << endl; exit(-1);}
    if (verbose) { cout << "*** Input ROOT file " << rootfile << " has been opened" << endl;}

    TTree   *tree       = (TTree*)f.Get("trainingtree;1");
    TBranch *branch     = tree->GetBranch("waveform");

    Int_t waveform[64][32];
    branch->SetAddress(&waveform);      // will read into this array
    Long64_t n = branch->GetEntries();  // number of entries in the branch
    if( N==0 || N>n ) { N=n; }          // decide how many to process, 0=all

    if(verbose) { std::cout << "*** Number of entries in the file: " << n << ", Number of entries to be processed: " << N << std::endl; }

    std::vector<float>          outputTensorValues(outputTensorSize);

    std::vector<const char*>    inputNames{inputName}, outputNames{outputName};
    std::vector<Ort::Value>     inputTensors, outputTensors;

    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

    std::vector<float> w31(inputTensorSize); // 31 bins in the test beam data

    // Example:
    // std::array<std::array<int, 3>, 3> arr = {{{5, 8, 2}, {8, 3, 1}, {5, 3, 9}}};

    // std::array<std::array<float,31>, 2> arr = {{
    //     {1554.0, 1558.0, 1555.0,  1564.0, 1558.0, 1555.0, 1556.0, 1554.0, 1750.0, 2284.0, 2424.0, 2116.0, 1838.0, 1713.0, 1649.0, 1613.0, 1601.0, 1589.0, 1583.0, 1578.0, 1572.0, 1574.0, 1573.0, 1569.0, 1567.0, 1562.0, 1563.0, 1560.0, 1561.0, 1557.0, 1557.0},
    //     {1554.0, 1558.0, 1555.0,  1564.0, 1558.0, 1555.0, 1556.0, 1554.0, 1750.0, 2284.0, 2424.0, 2116.0, 1838.0, 1713.0, 1649.0, 1613.0, 1601.0, 1589.0, 1583.0, 1578.0, 1572.0, 1574.0, 1573.0, 1569.0, 1567.0, 1562.0, 1563.0, 1560.0, 1561.0, 1557.0, 1557.0}
    // }};
    
    // for (auto &row: arr) {
    //     for (auto &i: row) {
    //         std::cout << i << ' ';
    //     }
    //     std::cout << std::endl;
    // }
    

    std::vector<float> arr = {
        1554.0, 1558.0, 1555.0,  1564.0, 1558.0, 1555.0, 1556.0, 1554.0, 1750.0, 2284.0, 2424.0, 2116.0, 1838.0, 1713.0, 1649.0, 1613.0, 1601.0, 1589.0, 1583.0, 1578.0, 1572.0, 1574.0, 1573.0, 1569.0, 1567.0, 1562.0, 1563.0, 1560.0, 1561.0, 1557.0, 1557.0,
        1554.0, 1558.0, 1555.0,  1564.0, 1558.0, 1555.0, 1556.0, 1554.0, 1750.0, 2284.0, 2424.0, 2116.0, 1838.0, 1713.0, 1649.0, 1613.0, 1601.0, 1589.0, 1583.0, 1578.0, 1572.0, 1574.0, 1573.0, 1569.0, 1567.0, 1562.0, 1563.0, 1560.0, 1561.0, 1557.0, 1557.0
    };


    std::vector<int64_t> inputDimsN     = {2,31};
    std::vector<int64_t> outputDimsN    = {2,3};


    std::vector<float>   outputTensorValuesN(6);
// static Value Ort::Value::CreateTensor	(	const OrtMemoryInfo * 	info,
//     T * 	p_data,
//     size_t 	p_data_element_count,
//     const int64_t * 	shape,
//     size_t 	shape_len 
//     )	


    inputTensors.push_back (Ort::Value::CreateTensor<float>(memoryInfo, arr.data(), 62,    inputDimsN.data(),   inputDims.size()));
    outputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, outputTensorValuesN.data(),  6, outputDimsN.data(),    outputDimsN.size()));
    
    cout << inputDims.size() << "!" << outputDimsN.size() << endl;

    oS->_session->Run(Ort::RunOptions{nullptr},
             inputNames.data(),  inputTensors.data(),    1,
             outputNames.data(), outputTensors.data(),   1);


    std::cout << outputTensorValues << std::endl;
    // for (int i=0; i<N; i++) {
    //     Int_t m = branch->GetEntry(i);
    //     auto start = chrono::high_resolution_clock::now();

    //     std::vector<int> inp;
    //     inp.insert(inp.begin(), std::begin(waveform[channel]), std::end(waveform[channel]));

    //     std::transform(inp.begin(), inp.end()-1, w31.begin(), [](int x) {return (float)x;});

    //     inputTensors.push_back (Ort::Value::CreateTensor<float>(memoryInfo, w31.data(),                 inputTensorSize,    inputDims.data(),   inputDims.size()));
    //     outputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, outputTensorValues.data(),  outputTensorSize, outputDims.data(),    outputDims.size()));

    //     // core inference
    //     oS->_session->Run(Ort::RunOptions{nullptr},
    //         inputNames.data(),  inputTensors.data(),    1,
    //         outputNames.data(), outputTensors.data(),   1);
        
    //     auto stop = chrono::high_resolution_clock::now();
    //     auto duration = chrono::duration_cast<microseconds>(stop - start);
    //     cout << "Microseconds: " << duration.count() << endl;
        
    //     std::cout << outputTensorValues << std::endl;
    // }
    exit(0);
}
