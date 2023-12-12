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


using namespace std;
using namespace OpenXLSX;
using namespace boost;

regex _VALID_CHARS ("a-zA-Z0-9\\_");

class Element {
    private:
        string __regulated;
        string __act;
        string __inh;
        int __value;
        int __levels;
        vector<int> __delays;
        vector<string> __balancing;
        int __spont_delay;
        int __noise;
        int __delta;
        bool __opt_input;
        bool __opt_output;
        string __opt_input_value;
        string __opt_output_value;
        string __opt_obj_weight;
        double __increment;
    
    public:
        Element() {};

        Element(string name, string A, string I, int value, int levels, vector<int> delays, vector<string> balancing, int spont_delay, int noise, int delta, bool opt_input, bool opt_output, string opt_input_value, string opt_output_value, string opt_obj_weight, double increment) {
            __regulated = name;
            __act = A;
            __inh = I;
            __value = value;
            __levels = levels;
            __delays = delays;
            __balancing = balancing;
            __spont_delay = spont_delay;
            __noise = noise;
            __delta = delta;
            __opt_input = opt_input;
            __opt_output = opt_output;
            __opt_input_value = opt_input_value;
            __opt_output_value = opt_output_value;
            __opt_obj_weight = opt_obj_weight;
            __increment = increment;
        }

        string get_name() {
            return __regulated;
        }

        int get_levels() {
            return __levels;
        }

        
        vector<string> get_name_list() {
            vector<string> name_list;
            name_list.push_back(__act);
            name_list.push_back(__inh);
            return name_list;
        }

        void set_value_index(int val) {
            return;
        }
};

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
                int noise = DEF_TAU_NOISE;
                int delta = DEF_DELAY_DELTA;
                bool opt_input = false;
                string opt_input_value = "";
                bool opt_output = false;
                string opt_output_value = "";
                string opt_obj_weight = "";

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
                vector<string> balancing;
                // #TODO add balance column support

                // Spont behavior
                vector<string> spont;

                // Get update group if element has regulators
                if (std::find(__updateList.begin(), __updateList.end(), X) != __updateList.end()) {
                    // code block
                }

            }



            for (auto &el : __getElement) {
                //
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
};

int main() {
    string model_file;
    cin >> model_file;
    Simulator sim(model_file);
    return 0;
}


