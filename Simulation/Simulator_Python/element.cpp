#include <regex>
#include <random>
#include <cmath>
#include <deque>
#include <unordered_map>
#include <vector>
#include <set>
#include <cassert>
#include <boost/algorithm/string.hpp>

using namespace std;

class Element {
private:
    std::string __regulated;
    std::string __act;
    std::string __inh;
    int __levels;
    std::vector<double> __levels_array;
    int __curr_val_index;
    int __curr_trend_index;
    int __next_val_index;
    int __increment;
    std::string __balance;
    int __curr_balancing_delay;
    std::string __spont;
    int __curr_spont_delay;
    std::vector<int> __delays;
    int __max_delay;
    std::vector<int> __curr_delays;
    double __noise;
    double __delta;
    std::deque<int> __reg_score;
    int __last_update_step;
    std::vector<std::string> __name_list;
    std::vector<int> __table_prop_delays;
    int __table_curr_reg_delays;
    std::vector<int> __old_table_indices;
    std::string __update_method;
    std::unordered_map<std::string, double> __name_to_value;
    std::unordered_map<std::string, int> __name_to_index;
    std::unordered_map<std::string, double> __name_to_trend;
    std::unordered_map<std::string, int> __name_to_trend_index;
    std::unordered_map<std::string, double> __previous_value;
    std::unordered_map<std::string, int> __previous_index;
    bool __opt_input;
    double __opt_fixed_input;
    bool __opt_output;
    double __opt_fixed_output;
    double __opt_obj_weight;
public:
    Element() {};
    Element(std::string X, std::string A, std::string I, int curr_val_index, int levels, std::vector<int> delays, std::string balancing, std::string spont_delay, double noise, double delta, bool opt_input, bool opt_output, double opt_input_value, double opt_output_value, double opt_obj_weight, int increment) {
        __regulated = X;
        std::regex whitespace("\\s");
        __act = std::regex_replace(A, whitespace, "");
        __inh = std::regex_replace(I, whitespace, "");
        __levels = levels;
        __levels_array.resize(levels);
        double step = 1.0 / (levels - 1);
        for (int i = 0; i < levels; i++) {
            __levels_array[i] = i * step;
        }
        __curr_val_index = curr_val_index;
        __curr_trend_index = 0;
        __next_val_index = 0;
        __increment = increment;
        __balance = balancing;
        __curr_balancing_delay = 0;
        __spont = spont_delay;
        __curr_spont_delay = 0;
        __delays = delays;
        __max_delay = *std::max_element(delays.begin(), delays.end());
        __curr_delays.resize(delays.size(), 0);
        __noise = noise;
        __delta = delta;
        if (spont_delay != "" && balancing != "") {
            __max_delay = std::max({__max_delay, std::stoi(spont_delay), std::stoi(balancing.substr(1))});
        } else if (spont_delay == "" && balancing != "") {
            __max_delay = std::max(__max_delay, std::stoi(balancing.substr(1)));
        } else if (spont_delay != "" && balancing == "") {
            __max_delay = std::max(__max_delay, std::stoi(spont_delay));
        }
        __reg_score.resize(__max_delay + 1);
        __last_update_step = 0;
        __name_list = create_name_list(X, A, I);
        __table_prop_delays.resize(0);
        /*if (__act == A) {
            __name_list = create_name_list(X, A, I);
        } else {
            __name_list = A["Regulators"];
            __table_prop_delays = A["Prop_delays"];
            __table_curr_reg_delays = 0;
            __old_table_indices.resize(0);
            __update_method = A["Reset"];
        }*/
        __name_to_value.clear();
        __name_to_index.clear();
        __name_to_trend.clear();
        __name_to_trend_index.clear();
        __previous_value.clear();
        __previous_index.clear();
        __opt_input = opt_input;
        __opt_fixed_input = opt_input_value;
        __opt_output = opt_output;
        __opt_fixed_output = opt_output_value;
        __opt_obj_weight = opt_obj_weight;
    }
    std::string get_name() {
        return __regulated;
    }
    std::string get_act() {
        return __act;
    }
    std::string get_inh() {
        return __inh;
    }
    int get_levels() {
        return __levels;
    }
    std::vector<double> get_levels_array() {
        return __levels_array;
    }
    std::vector<std::string> get_name_list() {
        return __name_list;
    }
    int get_current_value_index() {
        return __curr_val_index;
    }
    int get_next_value_index() {
        return __next_val_index;
    }
    void set_next_value_index(int val) {
        __next_val_index = val;
    }
    double get_current_value() {
        return __levels_array[__curr_val_index];
    }
    double get_value_from_index(int index) {
        return __levels_array[index];
    }
    int get_index_from_value(double value) {
        for (int i = 0; i < __levels; i++) {
            if (__levels_array[i] == value) {
                return i;
            }
        }
        return -1;
    }
    std::vector<int> get_delays() {
        return __delays;
    }
    std::string get_spont() {
        return __spont;
    }
    std::string get_balancing() {
        return __balance;
    }
    void set_value_index(int val_index) {
        if (val_index < __levels) {
            __curr_val_index = val_index;
        } else {
            throw std::invalid_argument("Invalid value index for " + __regulated + ", must be < " + std::to_string(__levels) + ": " + std::to_string(val_index));
        }
    }
    void set_trend_index(int trend_index) {
        if (std::abs(trend_index) < __levels) {
            __curr_trend_index = trend_index;
        } else {
            throw std::invalid_argument("Invalid trend index for " + __regulated + ", must be < " + std::to_string(__levels) + ": " + std::to_string(trend_index));
        }
    }
    void set_prev_value(std::unordered_map<std::string, double> prev_dict) {
        for (auto& [key, val] : prev_dict) {
            __previous_value[key] = val;
        }
    }
    void set_prev_index(std::unordered_map<std::string, int> prev_dict) {
        for (auto& [key, val] : prev_dict) {
            __previous_index[key] = val;
        }
    }
    bool is_input() {
        return __opt_input;
    }
    double fixed_input() {
        return __opt_fixed_input;
    }
    double fixed_output() {
        return __opt_fixed_output;
    }
    bool is_output() {
        return __opt_output;
    }
    double objective_weight() {
        return __opt_obj_weight;
    }
    int get_trend_index() {
        return __curr_trend_index;
    }
    double get_trend_from_index(int index) {
        if (index > 0) {
            return __levels_array[index];
        } else {
            return -__levels_array[-index];
        }
    }
    double get_trend() {
        return get_trend_from_index(__curr_trend_index);
    }

    std::unordered_map<std::string, double> get_previous_value() {
        return __previous_value;
    }
    std::unordered_map<std::string, int> get_previous_index() {
        return __previous_index;
    }
    std::vector<std::string> create_name_list(std::string X, std::string A, std::string I) {
        std::vector<std::string> names;
        std::set<std::string> reg_set;
        std::regex valid_chars("[a-zA-Z0-9_=]+");
        std::regex valid_name("[a-zA-Z]");
        std::smatch match;
        std::regex_iterator<std::string::iterator> rit(A.begin(), A.end(), valid_chars);
        std::regex_iterator<std::string::iterator> rend;
        while (rit != rend) {
            std::string regulator = rit->str();
            if (std::regex_search(regulator, valid_name)) {
                if (regulator.find('=') != std::string::npos) {
                    size_t pos = regulator.find('=');
                    std::string reg_name = regulator.substr(0, pos);
                    std::string target = regulator.substr(pos + 1);
                    reg_set.insert(reg_name);
                } else {
                    reg_set.insert(regulator);
                }
            }
            rit++;
        }
        names.insert(names.end(), reg_set.begin(), reg_set.end());
        names.push_back(X);
        return names;
    }
    void update(std::unordered_map<std::string, Element> getElement, std::unordered_map<std::string, vector<double>> memo, int step = 0, std::string simtype = "sync") {
        __name_to_value.clear();
        __name_to_index.clear();
        __name_to_trend.clear();
        __name_to_trend_index.clear();
        for (std::string name : __name_list) {
            __name_to_trend[name] = getElement[name].get_current_value() - __previous_value[name];
            __name_to_trend_index[name] = getElement[name].get_current_value_index() - __previous_index[name];
            __name_to_value[name] = getElement[name].get_current_value();
            __name_to_index[name] = getElement[name].get_current_value_index();
        }
        __curr_val_index = evaluate(memo, step, simtype);
        set_prev_value(__name_to_value);
        set_prev_index(__name_to_index);
    }
    void update_next(std::unordered_map<std::string, Element> getElement, std::unordered_map<std::string, vector<double>> memo, int step = 0, std::string simtype = "sync") {
        __name_to_value.clear();
        __name_to_index.clear();
        __name_to_trend.clear();
        __name_to_trend_index.clear();
        for (std::string name : __name_list) {
            __name_to_trend[name] = getElement[name].get_current_value() - __previous_value[name];
            __name_to_trend_index[name] = getElement[name].get_current_value_index() - __previous_index[name];
            __name_to_value[name] = getElement[name].get_current_value();
            __name_to_index[name] = getElement[name].get_current_value_index();
        }
        __next_val_index = evaluate(memo, step, simtype);
        set_prev_value(__name_to_value);
        set_prev_index(__name_to_index);
    }
    int evaluate(std::unordered_map<std::string, vector<double>> memo, int step, std::string simtype) {
        int X_next_index = 0;
        std::string mapping = "increment";
        double y_act = eval_reg(__act, 0, memo, step)[0];
        double y_inh = eval_reg(__inh, 0, memo, step)[0];
        
        int max_value_index = __levels - 1;
        if (mapping == "increment") {
            int X_curr_index = __curr_val_index;
            int D_spont;
            int D_balancing;
            std::vector<int> D;
            if (simtype == "rand_sync" || simtype == "rand_sync_gauss") {
                if (__spont != "" && __spont != "0") {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(std::stoi(__spont) - __delta, std::stoi(__spont) + __delta);
                    int spont_dv = dis(gen);
                    if (simtype == "rand_sync_gauss") {
                        std::normal_distribution<> gauss_dis(std::stoi(__spont), __delta);
                        spont_dv = std::round(gauss_dis(gen));
                    }
                    D_spont = spont_dv < 0 ? 0 : spont_dv;
                } else {
                    D_spont = 0;
                }
                if (__balance.size() == 2 && __balance[0] != '0') {
                    int balancing = std::stoi(__balance.substr(0, 1));
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(std::stoi(__balance.substr(1)) - __delta, std::stoi(__balance.substr(1)) + __delta);
                    int balancing_dv = dis(gen);
                    if (simtype == "rand_sync_gauss") {
                        std::normal_distribution<> gauss_dis(std::stoi(__balance.substr(1)), __delta);
                        balancing_dv = std::round(gauss_dis(gen));
                    }
                    D_balancing = balancing_dv < 0 ? 0 : balancing_dv;
                } else {
                    D_balancing = 0;
                }
                D.resize(__delays.size());
                for (int i = 0; i < __delays.size(); i++) {
                    if (__delays[i] != 0) {
                        std::random_device rd;
                        std::mt19937 gen(rd());
                        std::uniform_int_distribution<> dis(__delays[i] - __delta, __delays[i] + __delta);
                        int new_dv = dis(gen);
                        if (simtype == "rand_sync_gauss") {
                            std::normal_distribution<> gauss_dis(__delays[i], __delta);
                            new_dv = std::round(gauss_dis(gen));
                        }
                        D[i] = new_dv < 0 ? 0 : new_dv;
                    } else {
                        D[i] = 0;
                    }
                }
            } else {
                D_spont = std::stoi(__spont);
                if (__balance.size() == 2) {
                    int balancing = std::stoi(__balance.substr(0, 1));
                    D_balancing = std::stoi(__balance.substr(1));
                } else {
                    D_balancing = 0;
                }
                D = __delays;
            }
            
            if (__last_update_step > 0) {
                int step_diff = step - __last_update_step;
                if (step_diff > 1 && step > 1) {
                    int last_reg_score = __reg_score.back();
                    int stp = 1;
                    while (stp < step_diff) {
                        __reg_score.push_back(last_reg_score);
                        __curr_delays[X_curr_index] += 1;
                        if (D_spont != 0) {
                            __curr_spont_delay += 1;
                        }
                        if (__balance != "") {
                            __curr_balancing_delay += 1;
                        }
                        stp += 1;
                    }
                }
            }
            __last_update_step = step;
            
            std::vector<int> reg_scores_increase;
            if (D[X_curr_index] > 0 && __reg_score.size() >= D[X_curr_index]) {
                std::vector<int> delay_index_increase(D[X_curr_index]);
                for (int i = 0; i < D[X_curr_index]; i++) {
                    delay_index_increase[i] = -i - 1;
                }
                for (int indx : delay_index_increase) {
                    reg_scores_increase.push_back(__reg_score[indx]);
                }
            }
            
            std::vector<int> reg_scores_decrease;
            if (X_curr_index > 0 && D[-X_curr_index] > 0 && __reg_score.size() >= D[-X_curr_index]) {
                std::vector<int> delay_index_decrease(D[-X_curr_index]);
                for (int i = 0; i < D[-X_curr_index]; i++) {
                    delay_index_decrease[i] = -i - 1;
                }
                for (int indx : delay_index_decrease) {
                    reg_scores_decrease.push_back(__reg_score[indx]);
                }
            }
            
            std::vector<int> reg_scores_balancing;
            if (D_balancing != 0 && __reg_score.size() >= D_balancing) {
                std::vector<int> delay_index_balancing(D_balancing);
                for (int i = 0; i < D_balancing; i++) {
                    delay_index_balancing[i] = -i - 1;
                }
                for (int indx : delay_index_balancing) {
                    reg_scores_balancing.push_back(__reg_score[indx]);
                }
            }
            
            std::vector<int> reg_scores_spont;
            if (D_spont != 0 && __reg_score.size() >= D_spont) {
                std::vector<int> delay_index_spont(D_spont);
                for (int i = 0; i < D_spont; i++) {
                    delay_index_spont[i] = -i - 1;
                }
                for (int indx : delay_index_spont) {
                    reg_scores_spont.push_back(__reg_score[indx]);
                }
            }
            
            int increment = 1;
            int spont_increment = 1;
            int balance_increment = 1;
            if (__increment != 0) {
                double reg_score;
                if (y_inh == 0) {
                    reg_score = y_act;
                } else if (y_act == 0) {
                    reg_score = y_inh;
                } else {
                    reg_score = std::abs(y_act - y_inh);
                }
                double increment_float = __increment * reg_score * max_value_index;
                increment = std::ceil(increment_float);
                spont_increment = max_value_index;
                balance_increment = max_value_index;
            }
            
            if (__act != "" && __inh == "") {
                if (y_act > 0) {
                    int score_one = 0;
                    for (int score : reg_scores_increase) {
                        if (score == 1) {
                            score_one++;
                        }
                    }
                    if (score_one >= (D[X_curr_index] - __noise) && __curr_delays[X_curr_index] >= D[X_curr_index]) {
                        X_next_index = X_curr_index + increment;
                        __curr_delays[X_curr_index] = 0;
                    } else {
                        X_next_index = X_curr_index;
                        __curr_delays[X_curr_index] += 1;
                    }
                    __reg_score.push_back(1);
                } else if (y_act == 0) {
                    int score_zero = 0;
                    for (int score : reg_scores_spont) {
                        if (score == 0) {
                            score_zero++;
                        }
                    }
                    if (D_spont != 0) {
                        if (score_zero >= (D_spont - __noise) && __curr_spont_delay >= D_spont) {
                            X_next_index = X_curr_index - spont_increment;
                            __curr_spont_delay = 0;
                        } else {
                            X_next_index = X_curr_index;
                            __curr_spont_delay += 1;
                        }
                    } else {
                        X_next_index = X_curr_index;
                    }
                    __reg_score.push_back(0);
                }
            } else if (__act == "" && __inh != "") {
                if (y_inh > 0) {
                    int score_neg_one = 0;
                    for (int score : reg_scores_decrease) {
                        if (score == -1) {
                            score_neg_one++;
                        }
                    }
                    if (score_neg_one >= (D[-X_curr_index] - __noise) && __curr_delays[-X_curr_index] >= D[-X_curr_index]) {
                        X_next_index = X_curr_index - increment;
                        __curr_delays[-X_curr_index] = 0;
                    } else {
                        X_next_index = X_curr_index;
                        __curr_delays[-X_curr_index] += 1;
                    }
                    __reg_score.push_back(-1);
                } else if (y_inh == 0) {
                    int score_zero = 0;
                    for (int score : reg_scores_spont) {
                        if (score == 0) {
                            score_zero++;
                        }
                    }
                    if (D_spont != 0) {
                        if (score_zero >= (D_spont - __noise) && __curr_spont_delay >= D_spont) {
                            X_next_index = X_curr_index + spont_increment;
                            __curr_spont_delay = 0;
                        } else {
                            X_next_index = X_curr_index;
                            __curr_spont_delay += 1;
                        }
                    } else {
                        X_next_index = X_curr_index;
                    }
                    __reg_score.push_back(0);
                }
            } else if (__act != "" && __inh != "") {
                if (y_act > y_inh) {
                    int score_one = 0;
                    for (int score : reg_scores_increase) {
                        if (score == 1) {
                            score_one++;
                        }
                    }
                    if (score_one >= (D[X_curr_index] - __noise) && __curr_delays[X_curr_index] >= D[X_curr_index]) {
                        X_next_index = X_curr_index + increment;
                        __curr_delays[X_curr_index] = 0;
                    } else {
                        X_next_index = X_curr_index;
                        __curr_delays[X_curr_index] += 1;
                    }
                    __reg_score.push_back(1);
                } else if (y_act == y_inh) {
                    int score_zero = 0;
                    for (int score : reg_scores_balancing) {
                        if (score == 0) {
                            score_zero++;
                        }
                    }
                    if (__balance != "") {
                        if (__balance.substr(0, 1) == "d") {
                            if (score_zero >= (D_balancing - __noise) && __curr_balancing_delay >= D_balancing) {
                                X_next_index = X_curr_index - balance_increment;
                                __curr_balancing_delay = 0;
                            } else {
                                X_next_index = X_curr_index;
                                __curr_balancing_delay += 1;
                            }
                        } else if (__balance.substr(0, 1) == "i") {
                            if (score_zero >= (D_balancing - __noise) && __curr_balancing_delay >= D_balancing) {
                                X_next_index = X_curr_index + balance_increment;
                                __curr_balancing_delay = 0;
                            } else {
                                X_next_index = X_curr_index;
                                __curr_balancing_delay += 1;
                            }
                        } else {
                            throw std::invalid_argument("Invalid balancing value " + __balance);
                        }
                    } else {
                        X_next_index = X_curr_index;
                    }
                    __reg_score.push_back(0);
                } else if (y_act < y_inh) {
                    int score_neg_one = 0;
                    for (int score : reg_scores_decrease) {
                        if (score == -1) {
                            score_neg_one++;
                        }
                    }
                    if (score_neg_one >= (D[-X_curr_index] - __noise) && __curr_delays[-X_curr_index] >= D[-X_curr_index]) {
                        X_next_index = X_curr_index - increment;
                        __curr_delays[-X_curr_index] = 0;
                    } else {
                        X_next_index = X_curr_index;
                        __curr_delays[-X_curr_index] += 1;
                    }
                    __reg_score.push_back(-1);
                }
            } else {
                X_next_index = X_curr_index;
            }
        } else {
            throw std::invalid_argument("Invalid update mapping");
        }
        int temp_out = std::min(X_next_index, max_value_index);
        return std::max(temp_out, 0);
    }

    std::vector<double> eval_reg(std::string reg_rule, int layer, std::unordered_map<std::string, std::vector<double>> memo, int step = 0) {
        std::vector<double> result;
        if (!reg_rule.empty()) {
            int N = this->__levels - 1;
            std::vector<double> y_init;
            std::vector<double> y_necessary;
            std::vector<double> y_enhance;
            std::vector<double> y_sum;
            std::vector<std::string> reg_list;
            double weight = 0;
            bool summation = false;
            if (reg_rule.find("+") == std::string::npos) {
                reg_list = this->split_comma_outside_parentheses(reg_rule);
            } else {
                boost::split(reg_list, reg_rule, boost::is_any_of("+"));
                summation = true;
            }

            for (const auto& reg_element : reg_list) {
                if (reg_element.front() == '{' && reg_element.back() == '}') {
                    assert(layer == 0);
                    if (reg_element.find("*") != std::string::npos) {
                        string name = reg_element.substr(1, reg_element.size() - 2).substr(reg_element.find("*") + 1);
                        double weight = std::stod(reg_element.substr(1, reg_element.size() - 2).substr(0, reg_element.find("*")));
                        std::vector<double> y = this->eval_reg(name, 1, memo, step);
                        std::transform(y.begin(), y.end(), y.begin(), [weight](double val) { return weight * val; });
                        y_init.insert(y_init.end(), y.begin(), y.end());
                    } else {
                        std::vector<double> y = this->eval_reg(reg_element.substr(1, reg_element.size() - 2), 1, memo, step);
                        y_init.insert(y_init.end(), y.begin(), y.end());
                    }
                } else if (reg_element.front() == '{' && reg_element.back() == ']') {
                    std::size_t cut_point = 0;
                    std::size_t parentheses = 0;
                    for (std::size_t index = 0; index < reg_element.size(); ++index) {
                        if (reg_element[index] == '{') {
                            ++parentheses;
                        } else if (reg_element[index] == '}') {
                            --parentheses;
                        }
                        if (parentheses == 0) {
                            cut_point = index;
                            break;
                        }
                    }
                    std::string necessary_element = reg_element.substr(1, cut_point - 1);
                    std::string enhance_element = reg_element.substr(cut_point + 2, reg_element.size() - 2 - cut_point);
                    
                    if (necessary_element.find("*") != std::string::npos) {
                        string name = (necessary_element.substr(necessary_element.find("*") + 1));
                        double weight = std::stod(necessary_element.substr(0, necessary_element.find("*"))); 
                        std::vector<double> y = this->eval_reg(name, 1, memo, step);
                        std::transform(y.begin(), y.end(), y.begin(), [weight](double val) { return weight * val; });
                        y_necessary.insert(y_necessary.end(), y.begin(), y.end());
                    } else {
                        std::vector<double> y = this->eval_reg(necessary_element, 1, memo, step);
                        y_necessary.insert(y_necessary.end(), y.begin(), y.end());
                    }
                    
                    if (enhance_element.find("*") != std::string::npos) {
                        string name = (enhance_element.substr(enhance_element.find("*") + 1));
                        double weight = std::stod(enhance_element.substr(0, enhance_element.find("*")));
                        std::vector<double> y = this->eval_reg(name, 1, memo, step);
                        std::transform(y.begin(), y.end(), y.begin(), [weight](double val) { return weight * val; });
                        y_enhance.insert(y_enhance.end(), y.begin(), y.end());
                    } else {
                        std::vector<double> y = this->eval_reg(enhance_element, 1, memo, step);
                        y_enhance.insert(y_enhance.end(), y.begin(), y.end());
                    }
                    
                    if (weight > 0) {
                        y_sum.push_back((y_necessary.empty() || std::all_of(y_necessary.begin(), y_necessary.end(), [](double val) { return val == 0; })) ? 0 : std::accumulate(y_necessary.begin(), y_necessary.end(), 0.0) + std::accumulate(y_enhance.begin(), y_enhance.end(), 0.0));
                    } else {
                        y_sum.push_back((y_necessary.empty() || std::all_of(y_necessary.begin(), y_necessary.end(), [](double val) { return val == 0; })) ? 0 : std::max({0.0, std::min(std::accumulate(y_necessary.begin(), y_necessary.end(), 0.0), *std::max_element(y_enhance.begin(), y_enhance.end())), 1.0}));
                    }

                } else if (reg_element.front() == '(' && reg_element.back() == ')') {
                    std::vector<double> y_and;
                    std::vector<std::string> and_entities = this->split_comma_outside_parentheses(reg_element.substr(1, reg_element.size() - 2));
                    for (const auto& and_entity : and_entities) {
                        std::vector<double> y = this->eval_reg(and_entity, 1, memo, step);
                        y_and.insert(y_and.end(), y.begin(), y.end());
                    }
                    y_sum.push_back(*std::min_element(y_and.begin(), y_and.end()));

                } else {
                    if (reg_element.find(",") != std::string::npos) {
                        throw std::invalid_argument("Found mixed commas (OR) and plus signs (ADD) in regulator function. Check for deprecated highest state notation element+ and replace with element^");
                    } else if (reg_element.back() == '+') {
                        throw std::invalid_argument("Check for deprecated highest state notation: replace element+ with element^");
                    } else {
                        if (reg_element.back() == '^') {
                            if (reg_element.front() == '!') {
                                y_sum.push_back((this->__name_to_value[reg_element.substr(1, reg_element.size() - 2)] == 1) ? 0 : 1);
                            } else {
                                y_sum.push_back((this->__name_to_value[reg_element.substr(0, reg_element.size() - 1)] == 1) ? 1 : 0);
                            }
                        } else if (reg_element.find("&") != std::string::npos) {
                            y_sum.push_back(this->tag_based_eval(reg_element));
                        } else if (reg_element.find("*") != std::string::npos) {
                            std::vector<double> multiplied_reg_values;
                            std::vector<std::string> multiplied_reg_list;
                            boost::split(multiplied_reg_list, reg_element, boost::is_any_of("*"));
                            for (const auto& reg : multiplied_reg_list) {
                                if (std::all_of(reg.begin(), reg.end(), [](char c) { return !std::isalpha(c); })) {
                                    multiplied_reg_values.push_back(std::stod(reg));
                                } else {
                                    multiplied_reg_values.push_back(this->eval_reg(reg, 1, memo, step)[0]);
                                }
                            }
                            y_sum.push_back(std::accumulate(multiplied_reg_values.begin(), multiplied_reg_values.end(), 1.0, std::multiplies<double>()));
                        } else {
                            y_sum.push_back(this->__name_to_value[reg_element]);
                        }
                    }
                }
            }
            if (layer == 0) {
                result.clear();
                if (this->__name_to_value[this->__regulated] == 0 && !y_init.empty() && std::all_of(y_init.begin(), y_init.end(), [](double val) { return val == 0; })) {
                    result.push_back(0);
                } else {
                    if (summation) {
                        result.push_back(std::accumulate(y_init.begin(), y_init.end(), 0.0) + std::accumulate(y_sum.begin(), y_sum.end(), 0.0));
                    } else {
                        if (!y_sum.empty() && !y_init.empty()) {
                            result.push_back(std::max(*std::max_element(y_init.begin(), y_init.end()), *std::max_element(y_sum.begin(), y_sum.end())));
                        } else if (y_sum.empty()) {
                            result.push_back(*std::max_element(y_init.begin(), y_init.end()));
                        } else if (y_init.empty()) {
                            result.push_back(*std::max_element(y_sum.begin(), y_sum.end()));
                        } else {
                            throw std::invalid_argument("Empty y_sum and y_init");
                        }
                    }
                }
            } else {
                result = y_sum;
            }
        }
        return result;
    }
    double evaluate_state(std::vector<int> state) {
        this->__name_to_value.clear();
        for (std::size_t reg_index = 0; reg_index < state.size(); ++reg_index) {
            this->__name_to_value[this->__name_list[reg_index]] = state[reg_index];
            this->__name_to_trend[this->__name_list[reg_index]] = 0.0;
        }
        std::unordered_map<std::string, std::vector<double>> memo;
        std::vector<double> y_act = this->eval_reg(this->__act, 0, memo);
        std::vector<double> y_inh = this->eval_reg(this->__inh, 0, memo);
        int X_curr_index = std::distance(this->__levels_array.begin(), std::find(this->__levels_array.begin(), this->__levels_array.end(), state.back()));
        int max_value_index = this->__levels - 1;
        std::string D_spont = this->__spont;
        std::string balancing = this->__balance;
        int D_balancing = std::stoi(balancing.substr(1));
        std::vector<int> D = this->__delays;
        int increment = 1;
        if (this->__increment != 0) {
            double reg_score = 0;
            if (!y_act.empty() && y_inh.empty()) {
                reg_score = y_act[0] * max_value_index;
            } else if (y_act.empty() && !y_inh.empty()) {
                reg_score = y_inh[0] * max_value_index;
            } else if (!y_act.empty() && !y_inh.empty()) {
                reg_score = std::abs(y_act[0] - y_inh[0]) * max_value_index;
            }
            double increment_float = this->__increment * reg_score;
            increment = std::ceil(increment_float);
        }
        int X_next_index = 0;
        if (!this->__act.empty() && this->__inh.empty()) {
            if (!y_act.empty()) {
                if (y_act[0] > 0) {
                    X_next_index = X_curr_index + increment;
                } else if (y_act[0] == 0) {
                    if (!D_spont.empty()) {
                        X_next_index = X_curr_index - 1;
                    } else {
                        X_next_index = X_curr_index;
                    }
                }
            }
        } else if (this->__act.empty() && !this->__inh.empty()) {
            if (!y_inh.empty()) {
                if (y_inh[0] > 0) {
                    X_next_index = X_curr_index - increment;
                } else if (y_inh[0] == 0) {
                    if (!D_spont.empty()) {
                        X_next_index = X_curr_index + 1;
                    } else {
                        X_next_index = X_curr_index;
                    }
                }
            }
        } else if (!this->__act.empty() && !this->__inh.empty()) {
            if (!y_act.empty() && !y_inh.empty()) {
                if (y_act[0] > y_inh[0]) {
                    X_next_index = X_curr_index + increment;
                } else if (y_act[0] == y_inh[0]) {
                    if (!balancing.empty()) {
                        if (balancing == "decrease" || balancing == "negative") {
                            X_next_index = X_curr_index - 1;
                        } else if (balancing == "increase" || balancing == "positive") {
                            X_next_index = X_curr_index + 1;
                        } else {
                            throw std::invalid_argument("Invalid balancing value " + balancing);
                        }
                    } else {
                        X_next_index = X_curr_index;
                    }
                } else if (y_act[0] < y_inh[0]) {
                    X_next_index = X_curr_index - increment;
                }
            }
        } else {
            X_next_index = X_curr_index;
        }
        int value_index = std::min(X_next_index, max_value_index);
        value_index = std::max(value_index, 0);
        double value = this->__levels_array[value_index];
        return value;
    }
    std::vector<std::vector<int>> generate_all_input_state(bool include_regulated = false) {
        std::size_t length = include_regulated ? this->__name_list.size() : this->__name_list.size() - 1;
        std::size_t levels = this->__levels;
        std::vector<std::vector<int>> total_states;
        for (std::size_t num = 0; num < std::pow(levels, length); ++num) {
            std::vector<int> this_state(length, 0);
            std::size_t temp = num;
            std::size_t bit_index = length - 1;
            while (temp > 0) {
                this_state[bit_index] = this->__levels_array[temp % levels];
                temp /= levels;
                --bit_index;
            }
            total_states.push_back(this_state);
        }
        return total_states;
    }
    void generate_element_expression(std::ostream& output_model_file) {
        if (this->__act.empty() && this->__inh.empty()) {
            return;
        } else {
            std::vector<std::vector<int>> input_states = this->generate_all_input_state(true);
            std::size_t bit_length = std::ceil(std::log2(this->__levels));
            std::vector<std::vector<std::string>> mode_to_expression(bit_length);
            for (const auto& state : input_states) {
                int value = this->evaluate_state(state);
                std::size_t k = 0;
                while (value > 0) {
                    if (std::fmod(value, 2)) {
                        mode_to_expression[k].push_back("(" + this->state_to_expression(state) + ")");
                    }
                    value /= 2;
                    ++k;
                }
            }
            output_model_file << "{\n";
            if (bit_length > 1) {
                for (std::size_t index = 0; index < bit_length; ++index) {
                    const auto& mode = mode_to_expression[index];
                    if (!mode.empty()) {
                        output_model_file << this->__regulated + "_" + std::to_string(index) + " = " + std::accumulate(mode.begin(), mode.end(), std::string(), [](const std::string& a, const std::string& b) { return a + " + " + b; }) + ";\n";
                    } else {
                        output_model_file << this->__regulated + "_" + std::to_string(index) + " = Const_False;\n";
                    }
                }
            } else {
                const auto& mode = mode_to_expression[0];
                if (!mode.empty()) {
                    output_model_file << this->__regulated + " = " + std::accumulate(mode.begin(), mode.end(), std::string(), [](const std::string& a, const std::string& b) { return a + " + " + b; }) + ";\n";
                } else {
                    output_model_file << this->__regulated + " = Const_False;\n";
                }
            }
            output_model_file << "}\n";
        }
    }
    std::string state_to_expression(const std::vector<int>& state) {
        std::vector<std::string> result;
        for (std::size_t index = 0; index < state.size(); ++index) {
            const std::string& element = this->__name_list[index];
            int value = static_cast<int>(state[index]);
            std::size_t bit_length = std::ceil(std::log2(this->__levels));
            for (std::size_t k = 0; k < bit_length; ++k) {
                if (std::fmod(value, 2)) {
                    result.push_back(element + "_" + std::to_string(k));
                } else {
                    result.push_back("!" + element + "_" + std::to_string(k));
                }
                value /= 2;
            }
        }
        return std::accumulate(result.begin(), result.end(), std::string(), [](const std::string& a, const std::string& b) { return a + "*" + b; });
    }
    double discrete_not(double x, int N) {
        assert(N >= x);
        return (N - x);
    }
    std::vector<std::string> split_comma_outside_parentheses(const std::string& sentence, char delimiter = ',') {
        std::vector<std::string> final_list;
        std::size_t parentheses = 0;
        std::size_t start = 0;
        for (std::size_t index = 0; index < sentence.size(); ++index) {
            if (index == sentence.size() - 1) {
                final_list.push_back(sentence.substr(start, index + 1 - start));
            } else if (sentence[index] == '(' || sentence[index] == '{' || sentence[index] == '[') {
                ++parentheses;
            } else if (sentence[index] == ')' || sentence[index] == '}' || sentence[index] == ']') {
                --parentheses;
            } else if (sentence[index] == delimiter && parentheses == 0) {
                final_list.push_back(sentence.substr(start, index - start));
                start = index + 1;
            }
        }
        return final_list;
    }
    double tag_based_eval(std::string regulation_string) {
        if (regulation_string.empty()) {
            return 0;
        }
        std::vector<std::string> reg_list;
        std::regex reg_expr("\\*");
        std::sregex_token_iterator iter(regulation_string.begin(), regulation_string.end(), reg_expr, -1);
        std::sregex_token_iterator end;
        while (iter != end) {
            reg_list.push_back(*iter);
            ++iter;
        }
        std::vector<std::string> tag_list(reg_list.size());
        for (int i = 0; i < reg_list.size(); i++) {
            if (std::regex_search(reg_list[i], std::regex("[A-Za-z_]"))) {
                tag_list[i] = "ELEMENT";
            } else {
                tag_list[i] = "WEIGHT";
            }
        }
        double mul_reg = 0;
        double mul_trend = 0;
        std::vector<double> mul_list(1, 1.0);
        for (int i = 0; i < reg_list.size(); i++) {
            if (tag_list[i] == "WEIGHT") {
                int and_count = std::count(reg_list[i].begin(), reg_list[i].end(), '&');
                if (and_count == 1) {
                    std::string trend_weight = reg_list[i].substr(0, reg_list[i].find('&'));
                    std::string reg_weight = reg_list[i].substr(reg_list[i].find('&') + 1);
                    try {
                        double trend_weight_val = std::stod(trend_weight);
                        double reg_weight_val = std::stod(reg_weight);
                        if (mul_reg != 0 || mul_trend != 0) {
                            mul_reg *= reg_weight_val;
                            mul_trend *= trend_weight_val;
                        } else {
                            mul_reg = reg_weight_val;
                            mul_trend = trend_weight_val;
                        }
                    } catch (const std::invalid_argument& e) {
                        throw std::invalid_argument("Invalid weights: weights need to be integers or floats.");
                    }
                } else if (and_count == 0) {
                    std::string reg_weight = reg_list[i];
                    double reg_weight_val;
                    try {
                        reg_weight_val = std::stod(reg_weight);
                    } catch (const std::invalid_argument& e) {
                        throw std::invalid_argument("Regulator weight must be a number. Simulator encountered: " + reg_weight);
                    }
                    if (mul_reg != 0) {
                        mul_reg *= reg_weight_val;
                    } else {
                        mul_reg = reg_weight_val;
                    }
                    mul_trend = 0;
                } else {
                    throw std::invalid_argument("Too many '&' signs: there can only be one or none.");
                }
            } else if (tag_list[i] == "ELEMENT") {
                int target_lvl = -1;
                if ((i > 0 && tag_list[i - 1] == "ELEMENT") || i == 0) {
                    mul_reg = 1;
                    mul_trend = 0;
                }
                std::string reg_string = reg_list[i];
                int not_gate = 0;
                if (reg_string[0] == '!') {
                    reg_string = reg_string.substr(1);
                    not_gate = 1;
                } else if (reg_string[0] == '?') {
                    reg_string = reg_string.substr(1);
                    not_gate = 2;
                }
                int max_val = 0;
                if (reg_string[reg_string.size() - 1] == '^') {
                    reg_string = reg_string.substr(0, reg_string.size() - 1);
                    max_val = 1;
                }
                std::smatch match;
                if (std::regex_search(reg_string, match, std::regex("=[0-9]+"))) {
                    std::string target_lvl_str = match.str().substr(1);
                    try {
                        target_lvl = std::stoi(target_lvl_str);
                    } catch (const std::invalid_argument& e) {
                        throw std::invalid_argument("Invalid target level for regulator " + reg_string);
                    }
                    reg_string = match.prefix();
                }
                double trend;
                try {
                    trend = __name_to_trend.at(reg_string);
                } catch (const std::out_of_range& e) {
                    throw std::invalid_argument("Invalid element name " + reg_string);
                }
                double value = __name_to_value.at(reg_string);
                if (not_gate == 2) {
                    assert(value <= 1);
                    value = (value == 0) ? 1 : 0;
                    trend *= -1;
                }
                if (target_lvl != -1) {
                    int idx = __name_to_index.at(reg_string);
                    value *= (idx == target_lvl) ? 1 : 0;
                    trend *= (idx == target_lvl) ? 1 : 0;
                }
                if (max_val == 1) {
                    value = (value == 1) ? 1 : 0;
                    trend *= (value == 1) ? 1 : 0;
                }
                if (not_gate == 1) {
                    assert(value <= 1);
                    value = 1 - value;
                    trend *= -1;
                }
                if (trend * mul_trend < 0) {
                    mul_trend = 0;
                }
                mul_list.push_back(mul_reg * value + mul_trend * trend);
                mul_reg = 0;
                mul_trend = 0;
            }
        }
        double result = 1.0;
        for (double mul : mul_list) {
            result *= mul;
        }
        return result;
    }
};

