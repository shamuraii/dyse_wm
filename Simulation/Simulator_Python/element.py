import re
import random
import math
import pandas as pd
import numpy as np
from Translation.model import get_model, model_to_dict
from collections import deque

class Element(object):
	def __init__(self,
			X, A, I,
			curr_val_index,
			levels, delays,
			balancing,
			spont_delay,
			noise,
			delta,
			opt_input,
			opt_output,
			opt_input_value,
			opt_output_value,
			opt_obj_weight,
			increment
			):

		# regulated element name
		self.__regulated = X.strip()

		# positive regulation (activation) function for this element
		# check for a truth table
		if type(A) is str:
			self.__act = re.sub('\s', '', A)
		elif type(A) is dict:
			self.__act = A['Table']
		else:
			raise ValueError('Invalid regulation function for ' + str(X))

		# negative regulation (inhibition) function for this element
		self.__inh = re.sub('\s', '', I)

		# number of discrete levels (states) for this element
		self.__levels = int(levels)

		# list of level values for this element
		self.__levels_array = np.linspace(0, 1, levels)

		# current element value
		# use the line below later if we decide to accept either index or float level
		# curr_val_index, = np.where(self.__levels_array == curr_val)
		self.__curr_val_index = int(curr_val_index)
		self.__curr_trend_index = 0

		# next element value. use in sync simulation to update all elements upon calculating next values for every element
		self.__next_val_index = 0

		# increment function for increasing/decreasing value
		self.__increment = increment

		# balancing behavior (for when regulation scores are equal)
		self.__balance = balancing
		self.__curr_balancing_delay = 0

		# delay values for spontaneous activation/inhibition for elements with no positive/negative regulators
		self.__spont = spont_delay
		self.__curr_spont_delay = 0

		# delay values for element level transition delays
		self.__delays = delays
		self.__max_delay = max(delays)
		self.__curr_delays = [0 for x in delays]

		# allowed noise
		self.__noise = noise

		# delay range in random-delay sync update scheme
		self.__delta = delta

		# creating a buffer with the same length as the max delay value
		if spont_delay != '' and balancing != '':
			max_delay = max(delays+[spont_delay]+[int(balancing[1])])
		elif spont_delay == '' and balancing != '':
			max_delay = max(delays+[int(balancing[1])])
		elif spont_delay != '' and balancing == '':
			max_delay = max(delays+[spont_delay])
		else:
			max_delay = max(delays)
		self.__reg_score = deque([],max_delay+1)

		# for sequential updates, we need to keep track of the step where the element was last updated
		self.__last_update_step = 0

		# names of this element and its regulators
		self.__name_list = list()
		self.__table_prop_delays = list()
		if type(A) is str:
			self.__name_list = self.create_name_list(X.strip(), A.strip(), I.strip())
		elif type(A) is dict:
			self.__name_list = A['Regulators']
			self.__table_prop_delays = A['Prop_delays']
			self.__table_reg_delays = A['Reg_delays']
			self.__table_curr_reg_delays = 0
			self.__old_table_indices = list()
			self.__update_method = A['Reset']
		else:
			raise ValueError('Invalid regulation function for ' + str(X))

		# dictionary mapping the names of this element and its regulators to their current values and indexes
		self.__name_to_value = dict()
		self.__name_to_index = dict()
		self.__name_to_trend = dict()
		self.__name_to_trend_index = dict()
		self.__previous_value = dict()
		self.__previous_index = dict()

		# check if the element is an input/output or not
		self.__opt_input = opt_input
		self.__opt_fixed_input = opt_input_value
		self.__opt_output = opt_output
		self.__opt_fixed_output = opt_output_value
		self.__opt_obj_weight = opt_obj_weight
	
	def get_trend(self):
		return self.get_trend_from_index(self.__curr_trend_index)

	def get_previous_value(self):
		return self.__previous_value

	def get_previous_index(self):
		return self.__previous_index

	def create_name_list(self, X, A, I):
		""" Create a list of this element, activator, and inhibitor names
			TODO: after using get_model in Simulator __init__ , just use the parsed lists returned in the model
		"""

		global _VALID_CHARS

		names = set([X])

		reg_set = set()
		for reg_func in [A, I]:
			reg_list = list(re.findall('['+_VALID_CHARS+'=]+', reg_func))
			for regulator in reg_list:
				# Ignore weights and target values and parse names only
				if re.search(r'[a-zA-Z]', regulator):
					if '=' in regulator:
						reg_name, target = regulator.split('=')
					else:
						reg_name = regulator
					reg_set.add(reg_name)

		# sorting and concatenating this way so that the regulated name is always at the end
		return sorted(list(reg_set-names)) + list(names)

	def update(self, getElement, memo=dict(), step=0, simtype='sync'):

		self.__name_to_value.clear()
		self.__name_to_index.clear()
		self.__name_to_trend.clear()
		self.__name_to_trend_index.clear()
		# update the state (values of this element and its regulators)
		for name in self.__name_list:
			# self.__name_to_trend[name] = getElement[name].get_trend()
			# self.__name_to_trend_index[name] = getElement[name].get_trend_index()
			self.__name_to_trend[name] = getElement[name].get_current_value() - self.get_previous_value()[name]
			self.__name_to_trend_index[name] = getElement[name].get_current_value_index() - self.get_previous_index()[name]
			self.__name_to_value[name] = getElement[name].get_current_value()
			self.__name_to_index[name] = getElement[name].get_current_value_index()

		self.__curr_val_index = self.evaluate(memo, step, simtype)
		self.set_prev_value({name: value for name,value in self.__name_to_value.items()})
		self.set_prev_index({name: value for name,value in self.__name_to_index.items()})

	def update_next(self, getElement, memo=dict(), step=0, simtype='sync'):

		self.__name_to_value.clear()
		self.__name_to_index.clear()
		self.__name_to_trend.clear()
		self.__name_to_trend_index.clear()
		# update the state (values of this element and its regulators)
		for name in self.__name_list:
			# self.__name_to_trend[name] = getElement[name].get_trend()
			# self.__name_to_trend_index[name] = getElement[name].get_trend_index()
			self.__name_to_trend[name] = getElement[name].get_current_value() - self.get_previous_value()[name]
			self.__name_to_trend_index[name] = getElement[name].get_current_value_index() - self.get_previous_index()[name]
			self.__name_to_value[name] = getElement[name].get_current_value()
			self.__name_to_index[name] = getElement[name].get_current_value_index()


		self.__next_val_index = self.evaluate(memo, step, simtype)
		self.__previous_value = {name: value for name,value in self.__name_to_value.items()}
		self.__previous_index = {name: value for name,value in self.__name_to_index.items()}


	def evaluate(self, memo=dict(), step=0, simtype=''):
		# method for calcuating next-state value from regulation scores
		mapping = None

		if type(self.__act) is str:
			# parse regulation function and calculate activation score
			y_act = self.eval_reg(self.__act, 0, memo, step)
			mapping = 'increment'
		elif type(self.__act) is np.ndarray:
			# will use truth table mapping to update this element
			mapping = 'table'
		else:
			raise ValueError('Invalid regulation function for ' + str(self.__regulated))

		if type(self.__inh) is str:
			y_inh = self.eval_reg(self.__inh, 0, memo, step)
		else:
			raise ValueError('Invalid regulation function for ' + str(self.__regulated))

		# define number of levels for code readability
		max_value_index = self.__levels-1

		if mapping == 'table':
			# get the next value from the element's truth table
			X_next_index = self.eval_table(memo, step)

		elif mapping == 'increment':
			# compare the regulation scores and increment the current value

			# define values for code readability
			X_curr_index = self.__curr_val_index

			# check whether to randomize delays
			if simtype == 'rand_sync' or simtype == 'rand_sync_gauss':
				# randomized delays for spontaneous behavior
				if self.__spont != '' and self.__spont != 0:
					spont_dv = random.randint(
						int(self.__spont)-self.__delta, int(self.__spont)+self.__delta)
					if simtype == 'rand_sync_gauss':
						spont_dv = int(round(np.random.normal(
							int(self.__spont), self.__delta, 1)))
					if spont_dv < 0:
						D_spont = 0
					else:
						D_spont = spont_dv
				elif self.__spont == '':
					D_spont = ''
				elif self.__spont == 0:
					D_spont = 0

				# randomized delays for balancing behavior
				if len(self.__balance) == 2 and self.__balance[0] !=0:
					balancing = self.__balance[0]
					balancing_dv = random.randint(
						int(self.__balance[1])-self.__delta, int(self.__balance[1])+self.__delta)
					if simtype == 'rand_sync_gauss':
						balancing_dv = int(
							round(np.random.normal(int(self.__balance[1]), self.__delta, 1)))
					if balancing_dv < 0:
						D_balancing = 0
					else:
						D_balancing = balancing_dv
				else:
					balancing = ''
					D_balancing = 0

				D = list()
				for dv in self.__delays:
					if dv != 0:
						new_dv = random.randint(dv-self.__delta, dv+self.__delta)
						if simtype == 'rand_sync_gauss':
							new_dv = int(round(np.random.normal(dv, self.__delta, 1)))
						if new_dv < 0:
							D.append(0)
						else:
							D.append(new_dv)
					else:
						D.append(0)
			else:
				# spontaneous behavior
				D_spont = self.__spont
				# get balancing behavior if defined in the model, or set to defaults
				if len(self.__balance) == 2:
					balancing = self.__balance[0]
					D_balancing = int(self.__balance[1])
				else:
					balancing = ''
					D_balancing = 0
				D = self.__delays

			# if step_diff > 1, then we use sequential updates and we need to hold the old regulation scores
			if self.__last_update_step > 0:
				step_diff = step - self.__last_update_step
				if step_diff > 1 and step > 1:
					last_reg_score = self.__reg_score[-1]
					stp = 1
					while stp < step_diff:
						self.__reg_score.append(last_reg_score)
						self.__curr_delays[int(X_curr_index)] += 1
						if D_spont != '':
							self.__curr_spont_delay += 1
						if balancing != '':
							self.__curr_balancing_delay += 1
						stp += 1

			self.__last_update_step = step

			if D[int(X_curr_index)] > 0 and len(self.__reg_score) >= D[int(X_curr_index)]:
				delay_index_increase = np.linspace(
					1, D[int(X_curr_index)]+1, D[int(X_curr_index)], dtype=int, endpoint=False)
				delay_index_increase = np.negative(np.transpose(delay_index_increase))
				reg_scores_increase = [self.__reg_score[indx]
									for indx in delay_index_increase]
			else:
				reg_scores_increase = []

			if int(X_curr_index) > 0 and D[-int(X_curr_index)] > 0 and len(self.__reg_score) >= D[-int(X_curr_index)]:
				delay_index_decrease = np.linspace(
					1, D[-int(X_curr_index)]+1, D[-int(X_curr_index)], dtype=int, endpoint=False)
				delay_index_decrease = np.negative(np.transpose(delay_index_decrease))
				reg_scores_decrease = [self.__reg_score[indx]
									for indx in delay_index_decrease]
			else:
				reg_scores_decrease =[]

			if D_balancing != '' and D_balancing > 0 and len(self.__reg_score) >= D_balancing:
				delay_index_balancing = np.linspace(1, D_balancing+1, D_balancing, dtype=int, endpoint=False)
				delay_index_balancing = np.negative(np.transpose(delay_index_balancing))
				reg_scores_balancing = [self.__reg_score[indx]
									for indx in delay_index_balancing]
			else:
				reg_scores_balancing = []

			if D_spont != '' and D_spont > 0 and len(self.__reg_score) >= D_spont:
				delay_index_spont = np.linspace(1, D_spont+1, D_spont, dtype=int, endpoint=False)
				delay_index_spont = np.negative(np.transpose(delay_index_spont))
				reg_scores_spont = [self.__reg_score[indx] for indx in delay_index_spont]
			else:
				reg_scores_spont = []

			increment = 1
			spont_increment = 1
			balance_increment = 1
			if self.__increment != 0:
				if y_inh is None:
					reg_score = y_act
				elif y_act is None:
					reg_score = y_inh
				else:
					reg_score = abs(y_act - y_inh)

				increment_float = float(self.__increment)*reg_score*max_value_index
				increment = int(np.ceil(increment_float))
				spont_increment = max_value_index
				balance_increment = max_value_index

			if (self.__act) and (not self.__inh):
				# this element has only positive regulators, increase if activation > 0, or spontaneously decay
				if (y_act > 0):
					score_one, = np.where(np.array(reg_scores_increase) == 1)
					# check the state transition delay value and increase
					if (len(score_one) >= (D[int(X_curr_index)]-self.__noise)) and (self.__curr_delays[int(X_curr_index)] >= D[int(X_curr_index)]):
						# increase and reset delay
						X_next_index = X_curr_index + increment
						self.__curr_delays[int(X_curr_index)] = 0
					else:
						# hold value and increment delay
						X_next_index = X_curr_index
						self.__curr_delays[int(X_curr_index)] += 1
					self.__reg_score.append(1)
				elif (y_act == 0):
					score_zero, = np.where(np.array(reg_scores_spont) == 0)
					if D_spont != '':
						# check spontaneous delay
						if (len(score_zero) >= (D_spont-self.__noise)) and (self.__curr_spont_delay >= D_spont):
							# spontaneously decay and reset spontaneous delay
							X_next_index = X_curr_index - spont_increment
							self.__curr_spont_delay = 0
						else:
							# hold value and increment spontaneous delay
							X_next_index = X_curr_index
							self.__curr_spont_delay += 1
					else:
						X_next_index = X_curr_index
					self.__reg_score.append(0)
			elif (not self.__act) and (self.__inh):
				# this element has only negative regulators, decrease if inhibition > 0, or spontaneously increase
				if (y_inh > 0):
					score_neg_one, = np.where(np.array(reg_scores_decrease) == -1)
					if (len(score_neg_one) >= (D[-int(X_curr_index)]-self.__noise)) and (self.__curr_delays[-int(X_curr_index)] >= D[-int(X_curr_index)]):
						# decrease and reset delay
						X_next_index = X_curr_index - increment
						self.__curr_delays[-int(X_curr_index)] = 0
					else:
						# hold value and increment delay
						X_next_index = X_curr_index
						self.__curr_delays[-int(X_curr_index)] += 1
					self.__reg_score.append(-1)
				elif (y_inh == 0):
					score_zero, = np.where(np.array(reg_scores_spont) == 0)
					if D_spont != '':
						# check spontaneous delay
						if (len(score_zero) >= (D_spont-self.__noise)) and (self.__curr_spont_delay >= D_spont):
							# spontaneously increase and reset spontaneous delay
							X_next_index = X_curr_index + spont_increment
							self.__curr_spont_delay = 0
						else:
							# hold value and increment spontaneous delay
							X_next_index = X_curr_index
							self.__curr_spont_delay += 1
					else:
						X_next_index = X_curr_index
					self.__reg_score.append(0)
			elif (self.__act) and (self.__inh):
				if (y_act > y_inh):
					score_one, = np.where(np.array(reg_scores_increase) == 1)
					# check the state transition delay value and increase
					if (len(score_one) >= (D[int(X_curr_index)]-self.__noise)) and (self.__curr_delays[int(X_curr_index)] >= D[int(X_curr_index)]):
						# increase and reset delay
						X_next_index = X_curr_index + increment
						self.__curr_delays[int(X_curr_index)] = 0
					else:
						# hold value and increment delay
						X_next_index = X_curr_index
						self.__curr_delays[int(X_curr_index)] += 1
					self.__reg_score.append(1)
				elif (y_act == y_inh):
					score_zero, = np.where(np.array(reg_scores_balancing) == 0)
					# check balancing behavior since regulator scores are equal
					if balancing != '':
						if balancing in ['decrease', 'negative']:
							# check balancing delay
							if (len(score_zero) >= (D_balancing-self.__noise)) and (self.__curr_balancing_delay >= D_balancing):
								# decay and reset balancing delay
								X_next_index = X_curr_index - balance_increment
								self.__curr_balancing_delay = 0
							else:
								# hold value and increment balancing delay
								X_next_index = X_curr_index
								self.__curr_balancing_delay += 1
						elif balancing in ['increase', 'positive']:
							# check balancing delay
							if (len(score_zero) >= (D_balancing-self.__noise)) and (self.__curr_balancing_delay >= D_balancing):
								# restore and reset balancing delay
								X_next_index = X_curr_index + balance_increment
								self.__curr_balancing_delay = 0
							else:
								# hold value and increment balancing delay
								X_next_index = X_curr_index
								self.__curr_balancing_delay += 1
						else:
							raise ValueError('Invalid balancing value ' + str(balancing))
					else:
						# no balancing behavior
						X_next_index = X_curr_index
					self.__reg_score.append(0)
				elif (y_act < y_inh):
					score_neg_one, = np.where(np.array(reg_scores_decrease) == -1)
					# check the state transition delay value and decrease
					if (len(score_neg_one) >= (D[-int(X_curr_index)]-self.__noise)) and (self.__curr_delays[-int(X_curr_index)] >= D[-int(X_curr_index)]):
						# decrease and reset delay
						X_next_index = X_curr_index - increment
						self.__curr_delays[-int(X_curr_index)] = 0
					else:
						# hold value and increment delay
						X_next_index = X_curr_index
						self.__curr_delays[-int(X_curr_index)] += 1
					self.__reg_score.append(-1)
			else:
				# this element has no regulators
				X_next_index = X_curr_index
		else:
			raise ValueError('Invalid update mapping')

		return sorted([0, X_next_index, max_value_index])[1]

	def eval_reg(self, reg_rule, layer, memo=dict(), step=0):

		# Only calculate the score if there are actually regulators for this element
		if reg_rule:

			N = self.__levels-1

			y_init = list()
			y_necessary = list()
			y_enhance = list()
			y_sum = list()
			weight = 0
			summation = False

			if '+' not in reg_rule:
				reg_list = self.split_comma_outside_parentheses(reg_rule)
			else:
				if ',' in reg_rule:
					reg_test = reg_rule.split(',')
					raise ValueError(
						'Found mixed commas (OR) and plus signs (ADD) in regulator function. Check for deprecated highest state notation element+ and replace with element^')
				elif reg_rule[-1] == '+':
					raise ValueError(
						'Check for deprecated highest state notation: replace element+ with element^')
				else:
					reg_list = reg_rule.split('+')
					# set the summation flag to indicate these should be summed
					summation = True

			# parse regulators, first checking for elements in {},  ()
			for reg_element in reg_list:

				if reg_element[0] == '{' and reg_element[-1] == '}':
					# this is an initializer
					# confirm that this is layer 0
					assert(layer == 0)
					# define as an initializer, evaluating only the expression within the brackets
					# check for a weight and multiply
					if '*' in reg_element:
						weight, name = reg_element[1:-1].split('*')
						y_init += float(weight)*self.eval_reg(name, 1, memo, step)
					else:
						y_init += self.eval_reg(reg_element[1:-1], 1, memo, step)

				elif reg_element[0] == '{' and reg_element[-1] == ']':
					# this is a necessary pair
					# Find the cut point between {} and []
					parentheses = 0
					cut_point = 0
					for index, char in enumerate(reg_element):
						if char == '{':
							parentheses += 1
						elif char == '}':
							parentheses -= 1
						if parentheses == 0:
							cut_point = index
							break

					necessary_element = reg_element[1:cut_point]
					enhance_element = reg_element[cut_point+2:-1]
					# define the first part as the necessary element
					# check for weights (asterisk notation indicating multiplication by weight)
					if '*' in necessary_element:
						weight, name = necessary_element.split('*')
						y_necessary += float(weight)*self.eval_reg(name, 1, memo, step)
					else:
						y_necessary += self.eval_reg(necessary_element, 1, memo, step)

					# define the second part as the enhancing/strengthening element
					# check for weights (asterisk notation indicating multiplication by weight)
					if '*' in enhance_element:
						weight, name = enhance_element.split('*')
						y_enhance += float(weight)*self.eval_reg(name, 1, memo, step)
					else:
						y_enhance += self.eval_reg(enhance_element, 1, memo, step)

					# increment the score according to the values of both the sufficient and enhancing elements
					# but use the 'sorted' expression to keep the value below the maximum value

					# TODO: need notation for necessary pair with sum
					# right now, uses sum if an element weight has been defined
					if float(weight) > 0:
						y_sum += [0 if all([y == 0 for y in y_necessary]) == True
													else float(sum(y_necessary)+sum(y_enhance))]
					else:
						y_sum += [0 if all([y == 0 for y in y_necessary]) == True
													else sorted([0, float(max(min(y_necessary), max(y_enhance))), 1])[1]]

				elif reg_element[0] == '(' and reg_element[-1] == ')':
					# this is a logic AND operation, all activators must be present
					# construct a list of the values of each element, then perform discrete logic AND (min)
					y_and = [float(x)
											for and_entity in self.split_comma_outside_parentheses(reg_element[1:-1])
											for x in self.eval_reg(and_entity, 1, memo, step)]
					y_sum += [min(y_and)]

				else:
					# single regulator
					# confirm that there are no commas remaining
					assert(',' not in reg_element)

					# calculate the value of the score based on the value of this regulator
					# using the element name to value mapping (__name_to_value dictionary)
					if reg_element[-1] == '^':
						# this is a highest state regulator
						# score is either zero or the maximum
						if reg_element[0] == '!':
							y_sum += [0 if self.__name_to_value[reg_element[1:-1]] == 1 else 1]
						else:
							y_sum += [1 if self.__name_to_value[reg_element[:-1]] == 1 else 0]

					elif '&' in reg_element:
						y_sum += [self.tag_based_eval(reg_element)]

					elif '*' in reg_element:
						# parse a list of values multiplied together
						multiplied_reg_values = list()
						multiplied_reg_list = reg_element.split('*')
						for reg in multiplied_reg_list:
							if not re.search(r'[a-zA-Z]', reg):
								# this is a weight, append the value directly
								multiplied_reg_values.append(float(reg))
							else:
								# this is an element, evaluate the value and then append
								multiplied_reg_values.append(
									float(self.eval_reg(reg, 1, memo, step)[0]))
						y_sum += [np.prod(np.array(multiplied_reg_values))]
					elif reg_element[0] == '!':
						# NOT notation, uses n's complement
						if '~' in reg_element[1:]:
							propagation_delay, name = reg_element[1:].split('~')
							propagation_delay = random.randint(
								int(propagation_delay)-self.__delta, int(propagation_delay)+self.__delta)
							if int(propagation_delay) < 0:
								propagation_delay = 0
							old_values = memo[name]
							if int(propagation_delay) < len(old_values) and int(propagation_delay) != 0:
								effective_value = int(old_values[step-int(propagation_delay)])
								y_sum += [float(self.discrete_not(float(effective_value/N), 1))]
							elif int(propagation_delay) != 0:
								y_sum += [float(self.discrete_not(float(old_values[0]/N), 1))]
							else:
								y_sum += [float(self.discrete_not(float(self.__name_to_value[name]), 1))]
						else:
							y_sum += [float(self.discrete_not(float(self.__name_to_value[reg_element[1:]]), 1))]
					elif '=' in reg_element:
						# check target value
						name, target_state = reg_element.split('=')
						# add 0 or highest level to the score
						if self.__name_to_index[name] == int(target_state):
							y_sum += [1]
						else:
							y_sum += [0]
					elif '~' in reg_element:
						# check propagation delay
						propagation_delay, name = reg_element.split('~')
						propagation_delay = random.randint(
							int(propagation_delay)-self.__delta, int(propagation_delay)+self.__delta)
						if int(propagation_delay) < 0:
							propagation_delay = 0
						old_values = memo[name]
						if int(propagation_delay) < len(old_values) and int(propagation_delay) != 0:
							y_sum += [float(old_values[step-int(propagation_delay)]/N)]
						elif int(propagation_delay) != 0:
							y_sum += [float(old_values[0]/N)]
						else:
							y_sum += [float(self.__name_to_value[name])]
					else:
						y_sum += [float(self.__name_to_value[reg_element])]

			if layer == 0:
				# check for initializers and value of intializers
				if (self.__name_to_value[self.__regulated] == 0
										and len(y_init) != 0
										and all([y == 0 for y in y_init]) == True):
					return 0
				else:
					# at the top layer, sum or discrete OR (max) based on whether
					# the groups were split on commas or +
					if summation:
						return sum(y_init) + sum(y_sum)
					else:
						if len(y_sum) > 0 and len(y_init) > 0:
							return max(max(y_init), max(y_sum))
						elif len(y_sum) == 0:
							return max(y_init)
						elif len(y_init) == 0:
							return max(y_sum)
						else:
							raise ValueError('Empty y_sum and y_init')

			else:
				return y_sum

	def evaluate_state(self, state):
		self.__name_to_value.clear()

		for reg_index, state_val in enumerate(state):
			# create a mapping between state names and state values
			self.__name_to_value[self.__name_list[reg_index]] = float(state_val)
			self.__name_to_trend[self.__name_list[reg_index]] = 0.0

		# calculate activation and inhibition scores
		y_act = self.eval_reg(self.__act, 0)
		y_inh = self.eval_reg(self.__inh, 0)

		# define current element value, levels, and max delays for code readability
		X_curr_index = self.__levels_array.tolist().index(state[-1])
		max_value_index = self.__levels-1

		# Ignoring delays for now, but need to know spontaneous behavior
		# TODO: include delays as variables in truth tables, and
		# merge this function with evaluate()
		D_spont = self.__spont
		if len(self.__balance) == 2:
			balancing = self.__balance[0]
			D_balancing = int(self.__balance[1])
		else:
			balancing = ''
			D_balancing = 0
		D = self.__delays

		increment = 1
		if self.__increment != 0:

			if y_act is not None and y_inh is None:
				reg_score = y_act*max_value_index
			elif y_act is None and y_inh is not None:
				reg_score = y_inh*max_value_index
			elif y_act is not None and y_inh is not None:
				reg_score = abs(y_act - y_inh)*max_value_index
			else:
				# no regulators
				reg_score = 0

			increment_float = float(self.__increment)*reg_score
			increment = int(np.ceil(increment_float))

		# determine next value of the regulated element,
		# based on the type of regulators and activation/inhibition scores
		if (self.__act) and (not self.__inh):
			# this element has only positive regulators, increase if activation > 0, or spontaneously decay
			if (y_act > 0):
				X_next_index = X_curr_index + increment
			elif (y_act == 0):
				if D_spont != '':
					# spontaneously decay
					X_next_index = X_curr_index - 1
				else:
					# no spontaneous behavior, hold value
					X_next_index = X_curr_index
		elif (not self.__act) and (self.__inh):
			# this element has only negative regulators, decrease if inhibition > 0, or spontaneously increase
			if (y_inh > 0):
				X_next_index = X_curr_index - increment
			elif (y_inh == 0):
				if D_spont != '':
					# spontaneously increase
					X_next_index = X_curr_index + 1
				else:
					# no spontaneous behavior, hold value
					X_next_index = X_curr_index
		elif (self.__act) and (self.__inh):
			if (y_act > y_inh):
				X_next_index = X_curr_index + increment
			elif (y_act == y_inh):
				if balancing != '':
					if balancing in ['decrease', 'negative']:
						X_next_index = X_curr_index - 1
					elif balancing in ['increase', 'positive']:
						X_next_index = X_curr_index + 1
					else:
						raise ValueError('Invalid balancing value ' + str(balancing))
				else:
					X_next_index = X_curr_index
			elif (y_act < y_inh):
				X_next_index = X_curr_index - increment
		else:
			# this element has no regulators;
			# Note that this shouldn't happen with the current model initialization
			X_next_index = X_curr_index

		# return the next state,
		# with a sort to keep the index within bounds
		value_index = sorted([0, X_next_index, max_value_index])[1]
		value = self.__levels_array[value_index]
		return value

	def generate_all_input_state(self, include_regulated=0):
		length = len(self.__name_list) if include_regulated else len(self.__name_list)-1
		levels = self.__levels
		total_states = []
		for num in range(int(math.pow(levels, length))):
			# generate the state
			this_state = [0]*length
			temp = num
			bit_index = -1
			while temp > 0:
				# converting the state values to the normalized range [0,1]
				this_state[bit_index] = self.__levels_array[temp % levels]
				temp = temp//levels  # integer division
				bit_index = bit_index - 1
			total_states.append(this_state)
		return total_states

	def generate_element_expression(self, output_model_file):
		if self.__act == '' and self.__inh == '':
			return None

		else:
			# generate truth table
			input_states = self.generate_all_input_state(1)

			bit_length = int(math.ceil(math.log(self.__levels, 2)))
			mode_to_expression = [[] for x in range(bit_length)]

			# define the value for each state
			for state in input_states:
				value = self.evaluate_state(state)
				for k in range(math.ceil(math.log(value+1, 2))):
					if value % 2:
						mode_to_expression[k].append('('+self.state_to_expression(state)+')')
					value = value//2

			# write the model to a txt file
			output_model_file.write('{\n')

			# only use the underscore bit# notation if there is more than one bit needed for this element's value
			if bit_length > 1:
				for index in range(bit_length):
					mode = mode_to_expression[index]
					if len(mode) != 0:
						output_model_file.write(self.__regulated+'_'+str(index)+' = ' +
													' + '.join(mode)+';\n')
					else:
						output_model_file.write(
							self.__regulated+'_'+str(index)+' = Const_False;\n')
			else:
				mode = mode_to_expression[0]
				if len(mode) != 0:
					output_model_file.write(self.__regulated+' = ' +
											' + '.join(mode)+';\n')
				else:
					output_model_file.write(self.__regulated+' = Const_False;\n')

			output_model_file.write('}\n')

	def state_to_expression(self, state):

		result = list()

		for index, state_val in enumerate(state):
			element = self.__name_list[index]
			value = int(state_val)

			bit_length = int(math.ceil(math.log(self.__levels, 2)))

			# only use underscore bit# notation if there is more than one bit needed for this element's value
			if bit_length > 1:
				for k in range(bit_length):
					if value % 2:
						result.append(element+'_'+str(k))
					else:
						result.append('!'+element+'_'+str(k))
					value = value//2
			else:
				if value % 2:
					result.append(element)
				else:
					result.append('!'+element)

		return '*'.join(result)

	def discrete_not(self, x, N):
		""" Compute NOT using n's complement
		"""

		assert N >= x, 'Can\'t compute NOT, input ({}) is greater than maximum value ({})'.format(x,N)

		return (N - x)

	def split_comma_outside_parentheses(self, sentence):
		""" Parse comma-separated strings in a regulation function,
			preserving groups of elements in parentheses or brackets (AND, necessary pair, initializers).
		"""
		final_list = list()
		parentheses = 0
		start = 0
		for index, char in enumerate(sentence):
			if index == len(sentence)-1:
				final_list.append(sentence[start:index+1])
			elif char == '(' or char == '{' or char == '[':
				parentheses += 1
			elif char == ')' or char == '}' or char == ']':
				parentheses -= 1
			elif (char == ',' and parentheses == 0):
				final_list.append(sentence[start:index])
				start = index+1
		return final_list

	def tag_based_eval(self, regulation_string):
		"""Trend-based regulation evaluation function. It is tag-based to allow multiplication
		as discrete disjunction."""
		if regulation_string=='':
			return 0

		# Split the product into factors
		reg_list = regulation_string.split('*')
		# Assign tags to discriminate between element and weight factors
		tag_list = ['ELEMENT' if re.search(r'[A-Za-z\_]',elem) else 'WEIGHT'
					for elem in reg_list]

		# Initialize calculations (mul_reg stores regular weights,
		# mul_trend stores trend weights, mul_list stores all factors for multiplication)
		mul_reg = 0
		mul_trend = 0
		mul_list = [1.]
		for i, tag in enumerate(tag_list):
			if tag=='WEIGHT':
				and_count = reg_list[i].count('&')
				if and_count==1:
					trend_weight, reg_weight = reg_list[i].split('&')
					try:
						trend_weight = float(trend_weight)
						reg_weight = float(reg_weight)
					except:
						raise ValueError('Invalid weights: weights need to be integers or floats.')
					if mul_reg!=0 or mul_trend!=0:
						mul_reg *= reg_weight
						mul_trend *= trend_weight
					else:
						mul_reg = reg_weight
						mul_trend = trend_weight
				elif and_count==0:
					trend_weight = 0
					try:
						reg_weight = float(reg_list[i])
					except:
						raise ValueError('Regulator weight must be a number. Simulator encountered: '+reg_list[i])
					if mul_reg!=0:
						mul_reg *= reg_weight
					else:
						mul_reg = reg_weight
					mul_trend = 0
				else:
					raise ValueError('Too many "&" signs: there can only be one or none.')
			elif tag=='ELEMENT':
				target_lvl = -1
				if (i>0 and tag_list[i-1]=='ELEMENT') or i==0:
					mul_reg = 1
					mul_trend = 0
				if reg_list[i][0]=='!':
					reg_string = reg_list[i][1:]
					not_gate = 1
				elif reg_list[i][0]=='?':
					reg_string = reg_list[i][1:]
					not_gate = 2
				else:
					reg_string = reg_list[i]
					not_gate = 0
				if reg_list[i][-1]=='^':
					reg_string = reg_string[:-1]
					max_val = 1
				else:
					max_val = 0
				if re.search(r'=[0-9]+',reg_string):
					try:
						reg_string, target_lvl = reg_string.split('=')
						target_lvl = int(target_lvl)
					except:
						raise ValueError('Invalid target level for regulator '+str(reg_string))
				try:
					trend = float(self.__name_to_trend[reg_string])

				except:
					raise ValueError('Invalid element name '+str(reg_string))

				value = self.__name_to_value[reg_string]

				if not_gate==2:
					assert value<=1, "Invalid normalized value (>1), Boolean negation requires it to be within the levels range."
					value = int(value==0)
					trend *= -1

				if target_lvl!=-1:
					idx = self.__name_to_index[reg_string]
					value *= int(idx==target_lvl)
					trend *= int(idx==target_lvl)

				if max_val==1:
					value = int(value==1)
					trend *= int(value==1)

				if not_gate==1:
					assert value<=1, "Invalid normalized value (>1), discrete negation requires it to be within the levels range."
					value = 1 - value
					trend *= -1

				if trend*mul_trend < 0:
					mul_trend = 0
				mul_list.append(mul_reg*value+mul_trend*trend)

				mul_reg = 0
				mul_trend = 0

		return np.prod(mul_list)
