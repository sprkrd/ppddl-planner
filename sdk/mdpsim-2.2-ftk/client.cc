/*
 * Copyright 2003-2005 Carnegie Mellon University and Rutgers University
 * Copyright 2007 H�kan Younes
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <config.h>
#include "client.h"
#if HAVE_SSTREAM
#include <sstream>
#else
#include <strstream>
namespace std {
typedef std::ostrstream ostringstream;
}
#endif
#include <unistd.h>
#include <cstdlib>


/* Extracts session request information. */
bool XMLClient::sessionRequestInfo(const XMLNode* node,
                               int& rounds, long& time, int& turns) {
  if (node == 0) {
    return false;
  }

  const XMLNode* settingNode = node->getChild("setting");
  if (!settingNode) {
    return false;
  }

  std::string s;
  if (!settingNode->dissect("rounds", s)) {
    return false;
  }
  rounds = atoi(s.c_str());

  if (!settingNode->dissect("allowed-time", s)) {
    return false;
  }
  time = atol(s.c_str());

  if (!settingNode->dissect("allowed-turns", s)) {
    return false;
  }
  turns = atoi(s.c_str());

  return true;
}


/* Extracts round request information. */
bool XMLClient::roundRequestInfo(const XMLNode* node,
		int& round_number, long& time_left, int& rounds_left) {

  if (!node || node->getName() != "round-init") {
	return false;
  }

  std::string s;
  if (!node->dissect("round", s)) {
    return false;
  }
  round_number = atoi(s.c_str());

  if (!node->dissect("time-left", s)) {
    return false;
  }
  time_left = atol(s.c_str());

  if (!node->dissect("rounds-left", s)) {
    return false;
  }
  rounds_left = atoi(s.c_str());

  return true;
}


/* Extracts an action from the given XML node. */
const Atom* XMLClient::getAtom(const Problem& problem, const XMLNode* atomNode) {
  if (atomNode == 0 || atomNode->getName() != "atom") {
    return 0;
  }

  std::string predicate_name;
  if (!atomNode->dissect("predicate", predicate_name)) {
    return 0;
  }
  const Predicate* p =
    problem.domain().predicates().find_predicate(predicate_name);
  if (p == 0) {
    return 0;
  }

  TermList terms;
  size_t argIndex = 0;
  for (int i = 0; i < atomNode->size(); i++) {
    const XMLNode* termNode = atomNode->getChild(i);
    if (termNode == 0 || termNode->getName() != "term") {
      continue;
    }
    if (argIndex >= PredicateTable::parameters(*p).size()) {
      return 0;
    }
    Type correctType = PredicateTable::parameters(*p)[argIndex];
    argIndex++;

    std::string term_name = termNode->getText();
    const Object* o = problem.terms().find_object(term_name);
    if (o != 0) {
      if (!TypeTable::subtype(TermTable::type(*o), correctType)) {
        return 0;
      }
    } else {
      o = problem.domain().terms().find_object(term_name);
      if (o == 0) {
        return 0;
      } else if (!TypeTable::subtype(TermTable::type(*o), correctType)) {
        return 0;
      }
    }

    terms.push_back(*o);
  }

  if (PredicateTable::parameters(*p).size() != terms.size()) {
    return 0;
  }

  return &Atom::make(*p, terms);
}


/* Extracts a fluent from the given XML node. */
const Fluent* XMLClient::getFluent(const Problem& problem,
                               const XMLNode* appNode) {
  if (appNode == 0 || appNode->getName() != "fluent") {
    return 0;
  }

  std::string function_name;
  if (!appNode->dissect("function", function_name)) {
    return 0;
  }
  const Function* f =
    problem.domain().functions().find_function(function_name);
  if (f == 0) {
    return 0;
  }

  TermList terms;
  size_t argIndex = 0;
  for (int i = 0; i<appNode->size(); i++) {
    const XMLNode* termNode = appNode->getChild(i);
    if (!termNode || termNode->getName() != "term") {
      continue;
    }
    if (argIndex >= FunctionTable::parameters(*f).size()) {
      return 0;
    }
    Type correctType = FunctionTable::parameters(*f)[argIndex];
    argIndex++;

    std::string term_name = termNode->getText();
    const Object* o = problem.terms().find_object(term_name);
    if (o != 0) {
      if (!TypeTable::subtype(TermTable::type(*o), correctType)) {
        return 0;
      }
    } else {
      o = problem.domain().terms().find_object(term_name);
      if (o == 0) {
        return 0;
      }
      else if (!TypeTable::subtype(TermTable::type(*o), correctType)) {
        return 0;
      }
    }

    terms.push_back(*o);
  }

  if (FunctionTable::parameters(*f).size() != terms.size()) {
    return 0;
  }

  return &Fluent::make(*f,terms);
}


/* Extracts a state from the given XML node. */
bool XMLClient::getState(AtomSet& atoms, ValueMap& values,
                     const Problem& problem, const XMLNode* stateNode) {
  if (stateNode == 0) {
    return false;
  }
  if (stateNode->getName() != "state") {
    return false;
  }

  for (int i = 0; i < stateNode->size(); i++) {
    const XMLNode* cn = stateNode->getChild(i);
    if (cn->getName() == "atom") {
      const Atom* atom = getAtom(problem, cn);
      if (atom != 0) {
        atoms.insert(atom);
        RCObject::ref(atom);
      }
    }
    else if (cn->getName() == "fluent") {
      const Fluent* fluent = getFluent(problem, cn);
      std::string value_str;
      if (!cn->dissect("value", value_str))
        return false;
      values.insert(std::make_pair(fluent, Rational(value_str.c_str())));
      RCObject::ref(fluent);
    }
  }

  return true;
}


/* Sends an action on the given stream. */
void XMLClient::sendAction(std::ostream& os, const Action* action) {
  if (action == 0) {
    os << "<done/>";
  } else {
    os << "<act>" << "<action><name>" << action->name() << "</name>";
    for (ObjectList::const_iterator oi = action->arguments().begin();
         oi != action->arguments().end(); oi++) {
      os << "<term>" << *oi << "</term>";
    }
    os << "</action></act>";
  }
}


/* ====================================================================== */
/* XMLClient */

/* Constructs an XML client */
XMLClient::XMLClient(Planner& planner, const Problem& problem,
                     const std::string& name, int fd) {
  planner.xml_client_ = this;
  std::ostringstream os;
  os.str("");
  os << "<session-request>"
     <<  "<name>" << name << "</name>"
     <<  "<problem>" << problem.name() << "</problem>"
     << "</session-request>";
#if !HAVE_SSTREAM
  os << '\0';
#endif
  write(fd, os.str().c_str(), os.str().length());

  const XMLNode* sessionInitNode = read_node(fd);

  if (!sessionRequestInfo(sessionInitNode,
                          total_rounds_, round_time_, round_turns_)) {
    std::cerr << "Error in server's session-request response" << std::endl;
    if (sessionInitNode != 0) {
      delete sessionInitNode;
    }
    return;
  }

  if (sessionInitNode != 0) {
    delete sessionInitNode;
  }

  int rounds_left = total_rounds_;
  while (rounds_left) {
    rounds_left--;
    os.str("");
    os << "<round-request/>";
#if !HAVE_SSTREAM
    os << '\0';
#endif
    write(fd, os.str().c_str(), os.str().length());
    const XMLNode* roundInitNode = read_node(fd);

    if (!roundRequestInfo(roundInitNode,
                                round_number_, time_left_, rounds_left_)) {
      std::cerr << "Error in server's session-request response" << std::endl;
      if (roundInitNode != 0) {
        delete roundInitNode;
      }
      return;
    }

    if (roundInitNode != 0) {
      delete roundInitNode;
    }

    planner.initRound();

    const XMLNode* response = 0;
    while (1) {

      if (response != 0) {
        delete response;
      }

      response = read_node(fd);

      if (!response) {
        std::cerr << "Invalid state response" << std::endl;
        return;
      }

      if (response->getName() == "end-round"
          || response->getName() == "end-session") {
        std::cout << response << std::endl;
        break;
      }

      AtomSet atoms;
      ValueMap values;
      if (!getState(atoms, values, problem, response)) {
        std::cerr << "Invalid state response: " << response << std::endl;
        delete response;
        return;
      }

      const Action *a = planner.decideAction(atoms, values);
      for (AtomSet::const_iterator ai = atoms.begin();
           ai != atoms.end(); ai++) {
        RCObject::destructive_deref(*ai);
      }
      for (ValueMap::const_iterator vi = values.begin();
           vi != values.end(); vi++) {
        RCObject::destructive_deref((*vi).first);
      }

      os.str("");
      sendAction(os, a);
#if !HAVE_SSTREAM
      os << '\0';
#endif
      write(fd, os.str().c_str(), os.str().length());
    }

    planner.endRound();

    if (response && response->getName() == "end-session") {
      delete response;
      break;
    }
    if (response != 0) {
      delete response;
    }
  }
  const XMLNode* endSessionNode = read_node(fd);

  if (endSessionNode) {
    std::cout << endSessionNode << std::endl;
    delete endSessionNode;
  }
}
