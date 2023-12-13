#include <regex>
#include <random>
#include <cmath>
#include <sstream>
#include <iostream>
#include <deque>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <OpenXLSX.hpp>
#include <boost/algorithm/string.hpp>

#include "element.cpp"

using namespace std;
using namespace OpenXLSX;
using namespace boost;

regex _VALID_CHARS ("a-zA-Z0-9\\_");

class Simulator {
    private:
        int DEF_LEVELS = 3;
        int DEF_INCREMENT = 1;
        int DEF_TAU_NOISE = 1;
        int DEF_DELAY_DELTA = 2;
        vector<string> DEF_BALANCING = {"decrease", "0"};
        int DEF_SPONT = 0;
        double DEF_UPDATE_PROBABILITY = 0.5;
        int DEF_UPDATE_RANK = 0;
        unordered_map<string, Element> __getElement;
        vector<string> __updateList;
        unordered_map<string, int> __initial;
        vector<vector<string>> __randomInitial;
        vector<vector<string>> __knockout;
        unordered_map<string, vector<int>> __switchStep;
        unordered_map<string, vector<int>> __switchValue;
        unordered_map<string, string> __groupUpdate;
        vector<string> __groups;
        unordered_map<string, double> __rateUpdate;
        vector<string> __rateUpdateList;
        double __totalPriority = 0;
        unordered_map<int, vector<string>> __rankUpdate;
        unordered_map<string, double> __probUpdate;
        vector<unordered_map<string, vector<vector<int>>>> __exp_data_list;
    
        int getColIndex(string colName, vector<string> columns) {
            auto it = find(columns.begin(), columns.end(), colName);
            if (it == columns.end())
                return -1;
            else
                return (it - columns.begin());
        }

    public:
        Simulator(string model_file) {
            XLDocument doc;
            doc.open(model_file);
            auto wbk = doc.workbook();
            string name = wbk.worksheetNames()[0];
            auto sheet = wbk.worksheet(name);
            cout << sheet.rowCount() << endl;

            vector<XLCellValue> columnVals = sheet.row(1).values();
            vector<string> columnNames;
            for (int i=0; i<columnVals.size(); i++) {
                columnNames.push_back(algorithm::to_lower_copy(columnVals[i].get<string>()));
                cout << columnNames[i] << endl;
            }
            
            // Required columns
            int input_col_X = getColIndex("variable", columnNames);
            int input_col_A = getColIndex("positive", columnNames);
            int input_col_I = getColIndex("negative", columnNames);
            int input_col_initial = getColIndex("initial 0", columnNames);

            if (input_col_X == -1 || input_col_A == -1 || input_col_I == -1 || input_col_initial == -1)
                throw invalid_argument("Missing one or more required columns in input file.");

            // Parse each row of input model file
            set<string> prevNames;
            bool first = true;
            for(auto &row : sheet.rows()) {
                if (first) {
                    first = false;
                    continue;
                }
                vector<XLCellValue> vals = row.values();
                string X = vals[input_col_X];
                string A = vals[input_col_A];
                string I = vals[input_col_I];
                cout << "X=" << X << " A=" << A << " I=" << I << endl;
                if (prevNames.insert(X).second == false)
                    throw invalid_argument("Duplicate element: " + X);
                if(!regex_match(X, regex("[A-Za-z0-9_]+")))
                    throw invalid_argument("Invalid characters in variable name: " + X);

                int levels = DEF_LEVELS;
                int input_col_maxstate = getColIndex("levels", columnNames);
                if (input_col_maxstate != -1) {
                    if (vals[input_col_maxstate].type() != XLValueType::Empty)
                        levels = (int) vals[input_col_maxstate];
                }

                if (!(A.empty() and I.empty())) {
                    __updateList.push_back(X);
                } else {
                    // Look for truth tables in other sheets
                    // #TODO
                }

                // Optional Columns
                // #TODO
                int increment = DEF_INCREMENT;
                double noise = DEF_TAU_NOISE;
                double delta = DEF_DELAY_DELTA;
                bool opt_input = false;
                double opt_input_value = 0;
                bool opt_output = false;
                double opt_output_value = 0;
                double opt_obj_weight = 0;

                // Initial Values for element
                // #TODO support multiple scenarios
                string initial_value_input = vals[input_col_initial];
                vector<string> initial_value_split;
                boost::split(initial_value_split, initial_value_input, boost::is_any_of(","));
                if (initial_value_split.size() > 1) {
                    initial_value_input = initial_value_split[0];
                    vector<int> switchValue_temp;
                    vector<int> switchStep_temp;
                    for (int i = 1; i < initial_value_split.size(); i++) {
                        string t = initial_value_split[i];
                        switchValue_temp.push_back(stoi(t.substr(0, t.find("["))));
                        switchStep_temp.push_back(stoi(t.substr(t.find("[") + 1, t.find("]") - t.find("[") - 1)));
                    }
                    __switchValue[X] = switchValue_temp;
                    __switchStep[X] = switchStep_temp;
                } else {
                    __switchValue[X] = vector<int>();
                    __switchStep[X] = vector<int>();
                }
                // TODO add support for random/low/high starts
                int init_val = stoi(initial_value_input);
                /*
                if (boost::to_lower_copy(initial_value_input) == "r" || boost::to_lower_copy(initial_value_input) == "random") {
                    __randomInitial[scenario].push_back(X);
                    init_val = rand() % levels;
                } else if (boost::to_lower_copy(initial_value_input) == "l" || boost::to_lower_copy(initial_value_input) == "low") {
                    init_val = 0;
                } else if (boost::to_lower_copy(initial_value_input) == "m" || boost::to_lower_copy(initial_value_input) == "med" || boost::to_lower_copy(initial_value_input) == "medium" || boost::to_lower_copy(initial_value_input) == "middle" || boost::to_lower_copy(initial_value_input) == "moderate") {
                    init_val = levels / 2;
                } else if (boost::to_lower_copy(initial_value_input) == "h" || boost::to_lower_copy(initial_value_input) == "high") {
                    init_val = levels - 1;
                } else if (boost::to_lower_copy(initial_value_input) == "x") {
                    __knockout[scenario].push_back(X);
                    init_val = 0;
                } else if (initial_value_input != "") {
                    init_val = stoi(initial_value_input);
                } else {
                    throw std::invalid_argument("Missing scenario " + scenario + " initial value for element " + X);
                }
                */
                if (init_val < levels)
                    __initial[X] = init_val;
                else
                    throw invalid_argument("Invalid initial value for element " + X);
                // END SCENARIO LOOP HERE

                // #TODO: Get element experimental or historical data
                
                // Get element state transition delays
                vector<int> delays(2*(levels-1), 0);
                // #TODO support custom delays

                // Get balancing behavior
                string balancing;
                // #TODO add balance column support

                // Spont behavior
                string spont_delay;

                // Get update group if element has regulators
                // #TODO add update support
                double prob;
                if (std::find(__updateList.begin(), __updateList.end(), X) != __updateList.end()) {
                    prob = DEF_UPDATE_PROBABILITY;
                }

                __getElement[X] = Element(X, A, I, __initial[X], levels, delays, balancing,
                            spont_delay, noise, delta, opt_input, opt_output, opt_input_value,
                            opt_output_value, opt_obj_weight, increment);
            }

            for (auto & [key, el] : __getElement) {
                for (auto & reg : el.get_name_list()) {
                    if (__getElement.find(reg) == __getElement.end()) {
                        throw invalid_argument("Invalid regulator " + reg + " for element " + key);
                    }
                }
            }
        }

        unordered_map<string, Element> get_elements() {
            return __getElement;
        }
        
        unordered_map<string, int> get_initial() {
            return __initial;
        }
        
        vector<unordered_map<string, vector<vector<int>>>> get_exp_data() {
            return __exp_data_list;
        }
        
        void set_initial(int scenario=0) {
            for (auto const& element : __getElement) {
                string key = element.first;
                try
                {
                    int val = __initial[key];
                    __getElement[key].set_value_index(val);
                }
                catch(const std::exception& e)
                {
                    throw invalid_argument("Scenario " + to_string(scenario) + " does not exist in model (note scenario is zero-indexed, 0 is the first scenario)");
                }
            }
        }
        
        void set_random_initial(int scenario=0) {
            for (auto const& name : __randomInitial[scenario]) {
                int init_val_index = rand() % __getElement[name].get_levels();
                __getElement[name].set_value_index(init_val_index);
            }
        }
        
        void knockout(int scenario=0) {
            for (auto const& name : __knockout[scenario]) {
                throw invalid_argument("Knockout function is not yet supported: " + name);
            }
        }

        void run_simulation(std::string simtype,
                    int runs,
                    int simStep,
                    std::string outName,
                    int scenario=0,
                    int outMode=1,
                    bool normalize=false,
                    bool randomizeEachRun=false,
                    std::string eventTraces="",
                    bool progressReport=false) {
    
            if (simtype != "ra" && simtype != "round" && simtype != "sync" && simtype != "ra_multi" && simtype != "sync_multi" && simtype != "rand_sync" && simtype != "rand_sync_gauss" && simtype != "fixed_updates") {
                throw std::invalid_argument("Invalid simulation scheme, must be ra, round, sync, ra_multi, sync_multi, rand_sync, rand_sync_guess or fixed_updates");
            }
            
            std::map<int, std::vector<std::string>> updates;
            if (simtype == "fixed_updates") {
                std::ifstream event_traces_file(eventTraces);
                std::string line;
                std::vector<std::string> content;
                while (std::getline(event_traces_file, line)) {
                    boost::algorithm::trim(line);
                    content.push_back(line);
                }
                int numLines = content.size();
                for (int content_index = 0; content_index < numLines; content_index++) {
                    if (std::regex_match(content[content_index], std::regex("Run #\\d+"))) {
                        std::smatch match;
                        std::regex_search(content[content_index], match, std::regex("Run #(\\d+)"));
                        int run = std::stoi(match[1]);
                        std::regex_search(content[content_index+1], match, std::regex("Steps #(\\d+)"));
                        int steps = std::stoi(match[1]);
                        std::vector<std::string> update_steps;
                        for (int i = 0; i < steps; i++) {
                            update_steps.push_back(content[content_index+2+i]);
                        }
                        updates[run] = update_steps;
                    }
                }
                if (updates[runs].size() != simStep) {
                    std::cout << "Steps input does not match event trace file." << std::endl;
                    std::cout << "Using number of steps in event traces: " << updates[runs].size() << std::endl;
                    simStep = updates[runs].size();
                }
                runs += 1;
            }
            
            std::ofstream output_file(outName);
            
            set_initial(scenario);
            
            std::map<std::string, std::vector<int>> freq_sum;
            std::map<std::string, std::vector<int>> square_sum;
            
            std::vector<string> updated_element(simStep, "");
            for (auto& element : __getElement) {
                freq_sum[element.first] = std::vector<int>(simStep+1, 0);
                square_sum[element.first] = std::vector<int>(simStep+1, 0);
                if (normalize) {
                    int ele_value = element.second.get_current_value();
                    freq_sum[element.first][0] = ele_value * runs;
                    square_sum[element.first][0] = ele_value * ele_value * runs;
                } else {
                    int ele_val_index = element.second.get_current_value_index();
                    freq_sum[element.first][0] = ele_val_index * runs;
                    square_sum[element.first][0] = ele_val_index * ele_val_index * runs;
                }
            }
            
            if (simtype == "round") {
                int totalNumElements = __updateList.size();
            }
            
            std::vector<int> progress_ticks;
            if (progressReport) {
                for (int i = 0; i <= runs; i += runs/10) {
                    progress_ticks.push_back(i);
                }
            }
            
            for (int run = 0; run < runs; run++) {
                set_initial(scenario);
                if (simtype == "sync" || simtype == "sync_multi" || simtype == "rand_sync" || simtype == "rand_sync_guass" || randomizeEachRun == true) {
                    set_random_initial(scenario);
                }
                
                std::unordered_map<std::string, std::vector<double>> memo;
                std::unordered_map<std::string, std::vector<double>> memo_trend;
                
                for (auto& element : __getElement) {
                    if (normalize) {
                        memo[element.first] = {element.second.get_current_value()};
                    } else {
                        memo[element.first] = {(double) element.second.get_current_value_index()};
                    }
                    memo_trend[element.first] = {0};
                    element.second.set_trend_index(0);
                    bool found = false;
                    for (const auto& val : __updateList) {
                        if (val == element.first) {
                            found = true;
                            break;
                        }
                    }
                    if (found) {
                        for (const std::string& el_reg : element.second.get_name_list()) {
                            element.second.set_prev_value({{el_reg, __getElement[el_reg].get_current_value()}});
                            element.second.set_prev_index({{el_reg, __getElement[el_reg].get_current_value_index()}});
                        }
                    } else {
                        element.second.set_prev_value({{element.first, element.second.get_current_value()}});
                        element.second.set_prev_index({{element.first, element.second.get_current_value_index()}});
                    }
                }
                
                for (int step = 1; step <= simStep; step++) {
                    if (simtype == "fixed_updates") {
                        std::string element = updates[run][step-1];
                        std::string name = fixed_updates(memo, step, element);
                        __getElement[name].set_value_index(__getElement[name].get_next_value_index());
                        updated_element[step-1] = name;
                    }
                    if (simtype == "ra") {
                        std::string name = ra_update(memo, step);
                        if (__groupUpdate.find(name) != __groupUpdate.end()) {
                            for (auto& [key, el] : __groupUpdate) {
                                if (__groupUpdate[name] == __groupUpdate[key]) {
                                    update_next(key);
                                }
                            }
                            for (auto& [key, el] : __groupUpdate) {
                                if (__groupUpdate[name] == __groupUpdate[key]) {
                                    __getElement[key].set_value_index(__getElement[key].get_next_value_index());
                                }
                            }
                        } else {
                            __getElement[name].set_value_index(__getElement[name].get_next_value_index());
                        }
                        updated_element[step-1] = name;
                    } else if (simtype == "sync" || simtype == "rand_sync" || simtype == "rand_sync_gauss") {
                        sync_update(memo, step, simtype);
                    } else if (simtype == "ra_multi") {
                        if (!__groupUpdate.empty()) {
                            std::string element;
                            bool passed;
                            std::tie(element, passed) = prob_update_ra();
                            if (__groupUpdate.find(element) != __groupUpdate.end()) {
                                for (auto& [key, el] : __groupUpdate) {
                                    if (__groupUpdate[element] == __groupUpdate[key]) {
                                        if (passed) {
                                            update_next(key);
                                        } else {
                                            __getElement[key].set_next_value_index(__getElement[key].get_current_value_index());
                                        }
                                    }
                                }
                                for (auto& [key, el] : __groupUpdate) {
                                    if (__groupUpdate[element] == __groupUpdate[key]) {
                                        __getElement[key].set_value_index(__getElement[key].get_next_value_index());
                                    }
                                }
                            } else {
                                __getElement[element].set_value_index(__getElement[element].get_next_value_index());
                            }
                        } else {
                            std::string element;
                            bool passed;
                            std::tie(element, passed) = prob_update_ra();
                            __getElement[element].set_value_index(__getElement[element].get_next_value_index());
                        }
                    } else if (simtype == "sync_multi") {
                        if (!__groupUpdate.empty()) {
                            std::vector<std::string> alreadyUpdated;
                            for (auto& element : __updateList) {
                                if (std::find(alreadyUpdated.begin(), alreadyUpdated.end(), element) == alreadyUpdated.end()) {
                                    bool passed = prob_update(element);
                                    alreadyUpdated.push_back(element);
                                    if (__groupUpdate.find(element) != __groupUpdate.end()) {
                                        for (auto& [key, el] : __groupUpdate) {
                                            if (__groupUpdate[element] == __groupUpdate[key]) {
                                                if (passed) {
                                                    update_next(key);
                                                } else {
                                                    __getElement[key].set_value_index(__getElement[key].get_current_value_index());
                                                }
                                                alreadyUpdated.push_back(key);
                                            }
                                        }
                                    }
                                }
                            }
                            for (auto& element : __updateList) {
                                __getElement[element].set_value_index(__getElement[element].get_next_value_index());
                            }
                        } else {
                            for (auto& element : __updateList) {
                                bool passed = prob_update(element);
                            }
                            for (auto& element : __updateList) {
                                __getElement[element].set_value_index(__getElement[element].get_next_value_index());
                            }
                        }
                    } else if (simtype == "round") {
                        std::vector<int> rankList;
                        for (auto& rank : __rankUpdate) {
                            rankList.push_back(rank.first);
                        }
                        std::sort(rankList.begin(), rankList.end(), std::greater<int>());
                        for (auto& rank : rankList) {
                            std::vector<std::string> currentRankList = __rankUpdate[rank];
                            int n = currentRankList.size();
                            std::vector<int> randomList(n);
                            std::iota(randomList.begin(), randomList.end(), 0);
                            std::random_shuffle(randomList.begin(), randomList.end());
                            for (auto& j : randomList) {
                                std::string element = currentRankList[j];
                                __getElement[element].update(__getElement, memo, step, simtype);
                            }
                        }
                    }
                    
                    for (auto& element : __getElement) {
                        int ele_value;
                        if (normalize) {
                            ele_value = element.second.get_current_value();
                        } else {
                            ele_value = element.second.get_current_value_index();
                        }
                        memo[element.first].push_back(ele_value);
                        
                        if (element.first == updated_element[step-1]) {
                            memo_trend[element.first].push_back(memo[element.first].back() - memo[element.first][memo[element.first].size()-2]);
                            element.second.set_trend_index(memo_trend[element.first].back());
                        } else {
                            memo_trend[element.first].push_back(element.second.get_trend_index());
                        }
                        
                        freq_sum[element.first][step] += ele_value;
                        square_sum[element.first][step] += ele_value * ele_value;
                        
                        if (__switchStep.find(element.first) != __switchStep.end()) {
                            for (int index = 0; index < 1; index++) {
                                if (__switchStep[element.first][scenario] == step) {
                                    int toggle_val = __switchValue[element.first][scenario];
                                    int trend_current = toggle_val - element.second.get_current_value_index();
                                    element.second.set_trend_index(trend_current);
                                    element.second.set_value_index(toggle_val);
                                    if (normalize) {
                                        ele_value = element.second.get_current_value();
                                        memo[element.first][step] = ele_value;
                                        freq_sum[element.first][step] = ele_value * runs;
                                        square_sum[element.first][step] = ele_value * ele_value * runs;
                                    } else {
                                        int ele_val_index = element.second.get_current_value_index();
                                        memo[element.first][step] = ele_val_index;
                                        freq_sum[element.first][step] = ele_val_index * runs;
                                        square_sum[element.first][step] = ele_val_index * ele_val_index * runs;
                                    }
                                } else {
                                    element.second.set_trend_index(element.second.get_trend_index());
                                }
                                memo_trend[element.first][step] = element.second.get_trend_index();
                            }
                        }
                    }
                }
                
                if (outMode == 1) {
                    output_file << "Run #" << run << std::endl;
                    for (auto& name : __getElement) {
                        int out_level;
                        if (normalize) {
                            out_level = 2;
                        } else {
                            out_level = name.second.get_levels();
                        }
                        output_file << name.first << "|" << out_level << "|";
                        for (auto& x : memo[name.first]) {
                            output_file << x << " ";
                        }
                        output_file << std::endl;
                    }
                } else if (outMode == 2) {
                    if (run == 0) {
                        output_file << "# time ";
                        for (auto& name : __getElement) {
                            output_file << name.first << " ";
                        }
                        output_file << "step" << std::endl;
                    }
                    for (int step = 0; step < simStep; step++) {
                        output_file << step << " ";
                        for (auto& name : __getElement) {
                            output_file << memo[name.first][step] << " ";
                        }
                        output_file << step << std::endl;
                    }
                } else if (outMode == 7) {
                    output_file << "Run #" << run << std::endl;
                    for (auto& name : updated_element) {
                        output_file << name << " ";
                    }
                    output_file << std::endl;
                }
                
                if (progressReport) {
                    if (std::find(progress_ticks.begin(), progress_ticks.end(), run) != progress_ticks.end()) {
                        std::cout << "Simulation " << std::to_string(std::distance(progress_ticks.begin(), std::find(progress_ticks.begin(), progress_ticks.end(), run)) * 10) << "% complete..." << std::endl;
                    }
                }
            }
            
            if (progressReport) {
                std::cout << "Simulation 100% complete." << std::endl;
            }
            
            if (outMode != 2 && outMode != 7 && (simtype != "sync" || simtype != "sync_multi" || simtype != "rand_sync" || simtype != "rand_sync_gauss")) {
                if (outMode == 3) {
                    output_file << "Run #" << runs << std::endl;
                }
                
                output_file << "Frequency Summary:" << std::endl;
                for (auto& name : __getElement) {
                    int out_level;
                    if (normalize) {
                        out_level = 2;
                    } else {
                        out_level = name.second.get_levels();
                    }
                    output_file << name.first << "|" << out_level << "|";
                    for (auto& x : freq_sum[name.first]) {
                        output_file << x << " ";
                    }
                    output_file << std::endl;
                }
                
                output_file << std::endl << "Squares Summary:" << std::endl;
                for (auto& name : __getElement) {
                    int out_level;
                    if (normalize) {
                        out_level = 2;
                    } else {
                        out_level = name.second.get_levels();
                    }
                    output_file << name.first << "|" << out_level << "|";
                    for (auto& x : square_sum[name.first]) {
                        output_file << x << " ";
                    }
                    output_file << std::endl;
                }
            }
        }

    void update(std::string element) {
        if (std::find(__updateList.begin(), __updateList.end(), element) != __updateList.end()) {
            std::unordered_map<string, vector<double>> memo;
            __getElement[element].update(__getElement, memo);
        } else {
            std::cout << "Element has no regulators" << std::endl;
        }
    }

    void update_next(std::string element) {
        if (std::find(__updateList.begin(), __updateList.end(), element) != __updateList.end()) {
            std::unordered_map<string, vector<double>> memo;
            __getElement[element].update_next(__getElement, memo);
        } else {
            std::cout << "Element has no regulators" << std::endl;
        }
    }

    std::string ra_update(std::unordered_map<std::string, vector<double>> memo, int step) {
        if (__totalPriority > 0) {
            int priorityIndex = (int) rand() / RAND_MAX * __totalPriority;
            std::string element = __rateUpdateList[priorityIndex];
            __getElement[element].update_next(__getElement, memo, step);
            return element;
        } else {
            std::string element = __updateList[rand() % __updateList.size()];
            __getElement[element].update_next(__getElement, memo, step);
            return element;
        }
    }

    std::string fixed_updates(std::unordered_map<std::string, vector<double>> memo, int step, std::string element) {
        __getElement[element].update_next(__getElement, memo, step);
        return element;
    }

    void sync_update(std::unordered_map<std::string, vector<double>> memo, int step, std::string simtype) {
        for (std::string element : __updateList) {
            __getElement[element].update_next(__getElement, memo, step, simtype);
        }
        for (std::string element : __updateList) {
            __getElement[element].set_value_index(__getElement[element].get_next_value_index());
        }
    }

    bool prob_update(std::string element) {
        double prob = __probUpdate[element];
        if (prob > (double)rand() / RAND_MAX) {
            std::unordered_map<string, vector<double>> memo;
            __getElement[element].update_next(__getElement, memo);
            return true;
        }
        return false;
    }

    std::pair<std::string, bool> prob_update_ra() {
        std::string element;
        if (__totalPriority > 0) {
            int priorityIndex = rand() / RAND_MAX * __totalPriority;
            element = __rateUpdateList[priorityIndex];
        } else {
            element = __updateList[rand() % __updateList.size()];
        }
        double prob = __probUpdate[element];
        if (prob > (double)rand() / RAND_MAX) {
            std::unordered_map<string, vector<double>> memo;
            __getElement[element].update_next(__getElement, memo);
            return std::make_pair(element, true);
        }
        return std::make_pair(element, false);
    }

    void create_rules(std::string outputFilename, int scenario) {
        std::ofstream output_txt(outputFilename);
        std::set<std::string> all_names;
        std::stringstream my_string_buffer;
        for (auto& pair : __getElement) {
            std::string X = pair.second.get_name();
            std::cout << "Creating the rule for " << X << std::endl;
            all_names.insert(pair.second.get_name_list().begin(), pair.second.get_name_list().end());
            pair.second.generate_element_expression(my_string_buffer);
        }

        for (std::string name : all_names) {
            int bit_length = std::ceil(std::log2(__getElement[name].get_levels()));
            int this_initial = __initial[name];
            std::string this_initial_bin = std::bitset<32>(this_initial).to_string().substr(32 - bit_length);
            if (bit_length > 1) {
                for (int k = 0; k < bit_length; k++) {
                    if (this_initial_bin[bit_length - k - 1] == '0') {
                        output_txt << name << "_" << k << " = false" << std::endl;
                    } else if (this_initial_bin[bit_length - k - 1] == '1') {
                        output_txt << name << "_" << k << " = true" << std::endl;
                    } else {
                        throw std::invalid_argument("Error parsing initial value for " + name);
                    }
                }
            } else {
                if (this_initial_bin[0] == '0') {
                    output_txt << name << " = false" << std::endl;
                } else if (this_initial_bin[0] == '1') {
                    output_txt << name << " = true" << std::endl;
                } else {
                    throw std::invalid_argument("Error parsing initial value for " + name);
                }
            }
        }

        output_txt << std::endl << "Rules:" << std::endl;
        output_txt << my_string_buffer.str();
        output_txt.close();
    }

    void create_truth_tables(std::string outputBaseFilename, int scenario) {
        for (auto& pair : __getElement) {
            std::string regulated = pair.second.get_name();
            std::ofstream output_model(outputBaseFilename + "_" + regulated + ".txt");
            output_model << regulated << std::endl;
            std::vector<std::string> name_list = pair.second.get_name_list();
            for (int i = 0; i < name_list.size(); i++) {
                output_model << name_list[i] << std::endl;
                int levels = __getElement[name_list[i]].get_levels();
                output_model << levels << std::endl;
            }
            int max_reg_states = __getElement[regulated].get_levels();
            for (int i = 0; i < max_reg_states; i++) {
                output_model << i << std::endl;
            }
            std::vector<std::vector<int>> input_states = pair.second.generate_all_input_state();
            for (int i = 0; i < input_states.size(); i++) {
                output_model << input_states[i][0];
                for (int j = 1; j < input_states[i].size(); j++) {
                    output_model << "\t" << input_states[i][j];
                }
                output_model << std::endl;
            }
            output_model.close();
        }
    }

    /*
    std::map<std::string, std::vector<std::string>> parse_truth_table(std::map<std::string, std::vector<std::string>> truthTable, int levels) {
        std::map<std::string, std::vector<std::string>> table;
        table["Regulators"] = truthTable["Regulators"];
        table["Prop_delays"] = std::vector<std::string>(table["Regulators"].size(), 0);
        auto it = truthTable.begin();
        std::string reset = (++it)->first;
        if (reset == "reset" || reset == "r" || reset == "Reset" || reset == "R") {
            table["Reset"] = {"reset"};
        } else if (reset == "no-reset" || reset == "n" || reset == "No-reset" || reset == "no-Reset" || reset == "N" || reset == "no reset" || reset == "No reset" || reset == "no Reset") {
            table["Reset"] = {"no-reset"};
        } else {
            table["Reset"] = {"reset"};
        }
        for (int i = 0; i < table["Regulators"].size() - 1; i++) {
            if (table["Regulators"][i].find("~") != std::string::npos) {
                std::string propagation_delay = table["Regulators"][i].substr(0, table["Regulators"][i].find("~"));
                std::string reg_name = table["Regulators"][i].substr(table["Regulators"][i].find("~") + 1);
                table["Prop_delays"][i] = std::stoi(propagation_delay);
                table["Regulators"][i] = reg_name;
            }
        }
        std::map<std::vector<int>, int> temp_table;
        std::vector<std::string> temp_table_keys;
        for (auto &keyval : truthTable)
            temp_table_keys.push_back(keyval.first);
        temp_table_keys.erase(temp_table_keys.begin());
        temp_table_keys.erase(temp_table_keys.end() - 1);
        for (int i = 0; i < temp_table_keys.size(); i++) {
            std::vector<int> key;
            for (int j = 0; j < temp_table_keys[i].size(); j++) {
                key.push_back(std::stoi(temp_table_keys[i][j]));
            }
            temp_table[key] = std::stoi(truthTable[temp_table_keys[i]][temp_table_keys[i].size() - 1]);
        }
        std::vector<int> table_array_shape;
        for (int i = 0; i < temp_table_keys.size(); i++) {
            table_array_shape.push_back(temp_table_keys[i].size());
        }
        std::vector<std::vector<int>> table_array(table_array_shape, std::vector<int>(levels, 0));
        std::vector<std::vector<int>> reg_delays_array(table_array_shape, std::vector<int>(levels, 0));
        for (auto& pair : temp_table) {
            std::vector<int> key = pair.first;
            int value = pair.second;
            table_array[key] = value;
            reg_delays_array[key] = std::stoi(truthTable["regulation delays"][temp_table_keys.index(key)]);
        }
        table["Table"] = table_array;
        table["Reg_delays"] = reg_delays_array;
        return table;
    }
    */
    std::string get_expression_from_truth_table(std::map<std::string, std::vector<std::string>> truthTable, std::string A) {
        std::cout << "Truth table conversion to expression not yet supported" << std::endl;
        return A;
    }

};
/*
int main() {
    string model_file;
    cin >> model_file;
    Simulator sim(model_file);
    return 0;
}
*/

