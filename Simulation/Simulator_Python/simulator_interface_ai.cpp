#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <map>
#include <regex>
#include <cassert>
#include <ctime>
#include <cstdlib>
#include <iomanip>
#include <stdexcept>
#include <iterator>

// #include "simulator_ai.cpp"

#include <boost/algorithm/string/trim.hpp>
#include <boost/program_options.hpp>


using namespace std;

namespace po = boost::program_options;

// class Element {
// private:
//     string __act;
//     string __inh;
// public:
//     Element(string act, string inh) : __act(act), __inh(inh) {}
//     string get_act() { return __act; }
//     string get_inh() { return __inh; }
// };

// class Simulator {
// private:
//     string inputFilename;
//     map<string, Element> elements;
// public:
//     Simulator(string inputFilename) : inputFilename(inputFilename) {
//         // Read input file and initialize elements
//     }
//     map<string, Element> get_elements() { return elements; }
//     void create_rules(string outputFilename, int scenario) {
//         // Create rules and write to output file
//     }
//     void create_truth_tables(string outputBaseFilename, int scenario) {
//         // Create truth tables and write to output files
//     }
//     void run_simulation(string simScheme, int runs, int steps, string outputFilename, int scenario, int outMode, bool normalize, bool randomizeEachRun, string eventTracesFile, bool progressReport) {
//         // Run simulation and write to output file
//     }
// };

std::string join(std::vector<std::string> const &strings, std::string delim)
{
    std::stringstream ss;
    std::copy(strings.begin(), strings.end(),
        std::ostream_iterator<std::string>(ss, delim.c_str()));
    return ss.str();
}

void setup_and_create_rules(string inputFilename, string outputFilename, string scenarioString = "0") {
    vector<string> scenarios;
    stringstream ss(scenarioString);
    string scenario;
    while (getline(ss, scenario, ',')) {
        scenarios.push_back(scenario);
    }
    Simulator model(inputFilename);
    if (scenarios.size() > 1) {
        for (string this_scenario : scenarios) {
            cout << "Scenario: " << this_scenario << endl;
            string this_output_filename = outputFilename.substr(0, outputFilename.find_last_of('.')) + "_" + this_scenario + ".txt";
            model.create_rules(this_output_filename, stoi(this_scenario));
        }
    } else {
        cout << "Scenario: " << scenarios[0] << endl;
        string this_output_filename = outputFilename.substr(0, outputFilename.find_last_of('.')) + ".txt";
        model.create_rules(this_output_filename, stoi(scenarios[0]));
    }
}

void setup_and_create_truth_tables(string inputFilename, string outputBaseFilename, string scenarioString = "0") {
    vector<string> scenarios;
    stringstream ss(scenarioString);
    string scenario;
    while (getline(ss, scenario, ',')) {
        scenarios.push_back(scenario);
    }
    Simulator model(inputFilename);
    if (scenarios.size() > 1) {
        for (string this_scenario : scenarios) {
            cout << "Scenario: " << this_scenario << endl;
            string this_base_filename = outputBaseFilename + "_" + this_scenario;
            model.create_truth_tables(this_base_filename, stoi(this_scenario));
        }
    } else {
        cout << "Scenario: " << scenarios[0] << endl;
        model.create_truth_tables(outputBaseFilename, stoi(scenarios[0]));
    }
}

int calculate_simulation_steps(string inputFilename, int time_units, int scale_factor = 1) {
    Simulator model(inputFilename);
    map<string, Element> elements = model.get_elements();
    int num_elements = elements.size();
    for (auto element : elements) {
        if (element.second.get_act() == "" && element.second.get_inh() == "") {
            num_elements--;
        }
    }
    int steps = ceil(time_units * num_elements * scale_factor);
    return steps;
}

vector<int> calculate_toggle_steps(string inputFilename, vector<int> toggle_times, int scale_factor = 1) {
    Simulator model(inputFilename);
    map<string, Element> elements = model.get_elements();
    int num_elements = elements.size();
    for (auto element : elements) {
        if (element.second.get_act() == "" && element.second.get_inh() == "") {
            num_elements--;
        }
    }
    vector<int> toggle_steps;
    for (int this_toggle : toggle_times) {
        toggle_steps.push_back(ceil(this_toggle * num_elements * scale_factor));
    }
    return toggle_steps;
}

void setup_and_run_simulation(string inputFilename, string outputFilename, int steps = 1000, int runs = 100, string simScheme = "ra", int outputFormat = 3, string scenarioString = "0", bool normalize = false, bool randomizeEachRun = false, string event_traces_file = "", bool progressReport = false) {
    vector<string> scenarios;
    stringstream ss(scenarioString);
    string scenario;
    while (getline(ss, scenario, ',')) {
        scenarios.push_back(scenario);
    }
    Simulator model(inputFilename);
    string this_event_traces_file;
    if (scenarios.size() > 1) {
        for (string this_scenario : scenarios) {
            string this_output_filename = outputFilename.substr(0, outputFilename.find_last_of('.')) + "_" + this_scenario + ".txt";
            if (event_traces_file != "") {
                this_event_traces_file = event_traces_file.substr(0, event_traces_file.find_last_of('.')) + "_" + this_scenario + ".txt";
            } else {
                this_event_traces_file = "";
            }
            model.run_simulation(simScheme, runs, steps, this_output_filename, stoi(this_scenario), outputFormat, normalize, randomizeEachRun, this_event_traces_file, progressReport);
            cout << "Simulation scenario " << this_scenario << " complete" << endl;
        }
    } else {
        string this_output_filename = outputFilename.substr(0, outputFilename.find_last_of('.')) + ".txt";
        if (event_traces_file != "") {
            this_event_traces_file = event_traces_file.substr(0, event_traces_file.find_last_of('.')) + ".txt";
        } else {
            this_event_traces_file = "";
        }
        model.run_simulation(simScheme, runs, steps, this_output_filename, stoi(scenarios[0]), outputFormat, normalize, randomizeEachRun, this_event_traces_file, progressReport);
        cout << "Simulation scenario " << scenarios[0] << " complete" << endl;
    }
}

void calculate_trace_difference(vector<string> inputFilenames, string outputFilename) {
    if (inputFilenames.size() == 1) {
        throw invalid_argument("Cannot calculate difference with only one input file");
    }
    string first_file = inputFilenames[0];
    ifstream first(first_file);
    vector<string> first_trace_data;
    string line;
    // TODO: Check if leading/trailing whitespace matters
    while (getline(first, line)) {
        boost::algorithm::trim(line);
        first_trace_data.push_back(line);
    }
    bool transpose_format = false;
    if (first_trace_data[0][0] == '#') { 
        transpose_format = true;
    }

    for (int file_index = 1; file_index < inputFilenames.size(); file_index++) {
        string this_filename = inputFilenames[file_index];
        smatch scenario_index;
        regex_search(this_filename, scenario_index, regex("([0-9]+).txt"));
        string output_index = scenario_index.size() > 0 ? scenario_index[0] : to_string(file_index);
        
        ofstream output_file(outputFilename.substr(0, outputFilename.find_last_of('.')) + output_index + ".txt");
        ifstream this_file(this_filename);
        vector<string> this_trace_data;
        // TODO: Check if whitespace matters, using boost trim
        while (getline(this_file, line)) {
            boost::algorithm::trim(line);
            this_trace_data.push_back(line);
        }
        for (int content_index = 0; content_index < this_trace_data.size(); content_index++) {
            string current_trace = this_trace_data[content_index];
            if (current_trace.find("Frequency") != string::npos || current_trace.find("Run") != string::npos) {
                output_file << current_trace << endl;
            } else if (current_trace.find('#') != string::npos) { 
                if (current_trace != first_trace_data[content_index]) {
                    throw invalid_argument("Element names in trace files do not match");
                }
                output_file << current_trace << endl;
            } else {
                vector<string> this_trace_data_points;
                stringstream ss(current_trace);
                string point;
                while (getline(ss, point, ' ')) {
                    this_trace_data_points.push_back(point);
                }
                vector<string> first_trace_data_points;
                stringstream ss2(first_trace_data[content_index]);
                while (getline(ss2, point, ' ')) {
                    first_trace_data_points.push_back(point);
                }

                vector<string> temp_trace_data_points;
                if (transpose_format) {
                    temp_trace_data_points.push_back(this_trace_data_points[0]);
                    temp_trace_data_points.push_back(this_trace_data_points[1]);
                    for (int i = 2; i < this_trace_data_points.size() - 1; i++) {
                        temp_trace_data_points.push_back(to_string(stoi(this_trace_data_points[i]) - stoi(first_trace_data_points[i])));
                    }
                    temp_trace_data_points.push_back(this_trace_data_points.back());
                } else {
                    if (this_trace_data_points[0] != first_trace_data_points[0]) {
                        throw invalid_argument("Element indices in trace files do not match " + this_trace_data_points[0] + "\t" + first_trace_data_points[0]);
                    }
                    temp_trace_data_points.push_back(this_trace_data_points[0]);
                    for (int i = 1; i < this_trace_data_points.size(); i++) {
                        temp_trace_data_points.push_back(to_string(stoi(this_trace_data_points[i]) - stoi(first_trace_data_points[i])));
                    }
                }
                output_file << join(temp_trace_data_points, " ") << endl;
            }
        }
    }
}


// Function incorrect here and in python
void concatenate_traces(vector<string> inputFilenames, string outputFilename) {
    if (inputFilenames.size() == 1) {
        throw invalid_argument("Cannot concatenate with only one input file");
    }
    vector<vector<string>> all_traces;
    for (int file_index = 0; file_index < inputFilenames.size(); file_index++) {
        string this_filename = inputFilenames[file_index];
        ifstream this_file(this_filename);
        vector<string> traces;
        string line;
        while (getline(this_file, line)) {
            traces.push_back(line);
        }
        if (traces[0][0] != '#') { // TODO: Check if # or \t and if should be string.find
            throw invalid_argument("Invalid trace file format");
        }
        smatch scenario_index;
        regex_search(this_filename, scenario_index, regex("([0-9]+).txt"));
        string output_index = scenario_index.size() > 0 ? scenario_index[0] : to_string(file_index);
        ofstream output_file(outputFilename.substr(0, outputFilename.find_last_of('.')) + output_index + ".txt");
        for (int i = 0; i < traces.size(); i++) {
            if (traces[i][0] == '\t') {
                output_file << traces[i] << endl;
            } else {
                stringstream ss(traces[i]);
                string point;
                vector<string> points;
                while (getline(ss, point, ' ')) {
                    points.push_back(point);
                }
                // TODO: Check csv output format ??
                output_file << join(points, " ") << endl;
            }
        }
    }
}

// Utilizes viz, not worth it
// Never gets called
// map<string, vector<vector<int>>> trace_distributions(string model_file, string traces_file, int time_units, int scale_factor = 1, bool normalize = false) {
//     map<string, vector<vector<int>>> distributions;
//     map<string, vector<vector<int>>> traces = get_traces(traces_file);
//     for (int i = 0; i < calculate_toggle_steps(model_file, range(time_units + 1), 1).size(); i++) {
//         for (auto el : traces) {
//             vector<vector<int>> M;
//             for (auto v : el.second) {
//                 M.push_back(v);
//             }
//             distributions[el.first].push_back(M[calculate_toggle_steps(model_file, range(time_units + 1), 1)[i]]);
//             if (normalize) {
//                 for (int j = 0; j < distributions[el.first][i].size(); j++) {
//                     distributions[el.first][i][j] /= traces[el.first].size() - 1;
//                 }
//             }
//         }
//     }
//     return distributions;
// }

void get_input_args() {
    // Parse command line arguments
}

int main(int argc, char *argv[]) {

    // Parse command line arguments
    string input_filename;
    string output_filename;
    string sim_scheme;
    int runs;
    int steps;
    int output_format;
    string event_traces_file;
    bool normalize_output;
    string scenarios;
    bool difference;
    bool concatenate;
    bool randomize_each_run;
    bool timed;
    po::options_description desc("Run a simulation");
    desc.add_options()
        ("input_filename", po::value<string>(& input_filename), 
            "path and name of the input file to use for the simulation")
        ("output_filename", po::value<string>(& output_filename),
            "path and name to use for the output file")
        ("sim_scheme", po::value<string>(& sim_scheme)->default_value("ra"),
            "simulation scheme")
        ("runs", po::value<int>(& runs)->default_value(100),
            "number of simulation runs (repetitions)")
        ("steps", po::value<int>(& steps)->default_value(1000),
            "number of simulation steps (duration)")
        ("output_format", po::value<int>(& output_format)->default_value(0),
            "format of trace file output")
        ("event_traces_file", po::value<string>(& event_traces_file)->default_value(""), 
            "path and name to use for the event traces file")
        ("normalize_output", po::value<bool>(& normalize_output)->default_value(false), 
            "in traces file, normalize values to the range [0,1]")
        ("scenarios", po::value<string>(& scenarios)->default_value("0"), 
            "scenario index specifying which Initial column to use \nor comma-sparated list of scenarios indices \n")
        ("difference", po::value<bool>(& difference)->default_value(false), 
            "calculate the difference between traces for the first scenario \nand traces from any other scenarios")
        ("concatenate", po::value<bool>(& concatenate)->default_value(false), 
            "concatenate trace files for all scenarios")
        ("randomize_each_run", po::value<bool>(& randomize_each_run)->default_value(false), 
            "For sequential schemes, randomize initial values in each run.")
        ("timed", po::value<bool>(& timed)->default_value(false), 
            "log the total simulation time");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    // Check some argument formats
    if (concatenate && output_format != 2)
        throw invalid_argument("Concatenation only supported for output_format=2");
    if (difference && scenarios.length() < 2)
        throw invalid_argument("Need at least two scenarios to calculate the difference");
    if (concatenate && scenarios.length() < 2)
        throw invalid_argument("Need at least two scenarios to concatenate trace files");
    if (sim_scheme == "fixed_updates" && event_traces_file == "")
        throw invalid_argument("Need to specify the event_traces file");
    

    // Set default trace file format
    if (output_format == 0) {
        if (sim_scheme == "ra" || sim_scheme == "round" || sim_scheme == "ra_multi") {
            output_format = 3;
        } else if (sim_scheme == "sync" || sim_scheme == "sync_multi" || sim_scheme == "rand_sync" || sim_scheme == "rand_sync_gauss" || sim_scheme == "fixed_updates") {
            output_format = 1;
        } else {
            throw invalid_argument("Invalid simulation scheme, choose: ra, round, sync, ra_multi, sync_multi, or rand_sync");
        }
    }

    if (output_format == 3 && (sim_scheme == "sync" || sim_scheme == "sync_multi" || sim_scheme == "rand_sync" || sim_scheme == "rand_sync_guass")) {
        throw invalid_argument("Frequency summary (output format 3) is not supported for synchronous scheme");
    }

    if (output_format == 1 || output_format == 2 || output_format == 3 || output_format == 7) {
        // run simulation to get output traces
        cout << "Simulating..." << endl;
        clock_t start_time = clock();

        setup_and_run_simulation(input_filename, output_filename, steps, runs, sim_scheme, output_format, 
                                 scenarios, normalize_output, randomize_each_run, event_traces_file);

        if (timed) {
            cout << "--- " << (clock() - start_time) / (double)CLOCKS_PER_SEC << " seconds ---" << endl;
        }

        if (difference) {
            std::vector<std::string> trace_file_names;
            std::string base_filename = output_filename.substr(0, output_filename.find_last_of("."));

            stringstream ss(scenarios);
            string scenario;
            while (getline(ss, scenario, ',')) {
                std::string trace_file_name = base_filename + "_" + scenario + ".txt";
                trace_file_names.push_back(trace_file_name);
            }
            
            std::string diff_filename = base_filename + "_diff.txt";
            calculate_trace_difference(trace_file_names, diff_filename);
        }

        if (concatenate) {
            if (output_format != 2) {
                throw invalid_argument("Concatenation only supported for output format 2");
            } else {
                vector<string> trace_file_names;
                stringstream ss(scenarios);
                string scenario;
                while (getline(ss, scenario, ',')) {
                    if (difference){
                        trace_file_names.push_back(output_filename.substr(0, output_filename.find_last_of('.')) + "_diff" + scenario + ".txt");
                    }
                    else {
                        trace_file_names.push_back(output_filename.substr(0, output_filename.find_last_of('.')) + "_" + scenario + ".txt");
                    }
                }
                
                concatenate_traces(trace_file_names, output_filename.substr(0, output_filename.find_last_of('.')) + "_concat.txt");
            }
        }
    } else if (output_format >= 4 && output_format <= 6) {
        if (output_format != 5) {
            setup_and_create_truth_tables(input_filename, output_filename.substr(0, output_filename.find_last_of('.')), scenarios);
        }
        if (output_format != 6) {
            setup_and_create_rules(input_filename, output_filename.substr(0, output_filename.find_last_of('.')), scenarios);
        }
    } else {
        throw invalid_argument("Unrecognized output_format, use output_format of 1, 2, 3, 5");
    }
    return 0;
}


