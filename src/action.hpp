#pragma once

#include <string>
#include <variant>

#include "knowledge.hpp" //for location types

enum class goal_type {
	sleep
};

struct entity;
struct map;
enum class action_states {
	goal,
	in_location
};
struct action_state_in_location {
	fact_location_type type;
};
struct action_state_goal {
	goal_type type;
};
struct action_input_output
{
	action_states type;//could use data.index() as a type
	std::variant<action_state_in_location,action_state_goal> data; 
	bool operator==(action_input_output& other) const { return false; } //FIXME: do equals actually...
};
struct action
{
	std::string name;

	using action_func = int(*)(action*, map*, entity* ,entity* );
	action_func perform_action;

	boolean_statement<action_input_output> required;
	//cost
	std::vector<action_input_output> output;
};
struct plan
{
	std::vector<action> actions;
	//std::vector<int> choice_taken; could be used to have alternative plans
};
/*
Possible action trees(?):
	Cleaning job:
		Roam and tidy/clean:
			tidy: look for items not in "tidy" positions, e.g. chairs, boxes, etc...
			clean: empty bins, clean spills, collect misc garbage
		After roam, or when full of stuff, dump into main collection bin
	Gardening:
		Roam and randomly:
			Cut branches, rake, remove weeds
			If see fruit, collect
		Maybe also bring collected fruit back
	Maintainance:
		Roam and check equipment
		Wait in office and go to fix broken equipment/connections
	Manufacture: - maybe under maintanance
	Research:
		Grammar-gen the action tree to perform research
		E.g. Lab report needs: 
				X spectrogram
				Get sample
					????
				Perf Spectrogram
					Need equipment
				Type in report
					Need PC?
	Security:
		Hang out in office
		Answer calls
		Handle ruffles
		Handle dangeruos ppl (speciments, etc)
	
	Foodprep:
	HR:
		Brainwashing
	Medical:
		Do annual checkups
		Heal people
		Do surgery (needed or not)
	IT/Programmers:
	
*/