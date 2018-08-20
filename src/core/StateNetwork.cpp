/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013, 2014 Daniel Edler, Martin Rosvall

 For more information, see <http://www.mapequation.org>


 This file is part of Infomap software package.

 Infomap software package is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Infomap software package is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with Infomap software package.  If not, see <http://www.gnu.org/licenses/>.

**********************************************************************************/


#include "StateNetwork.h"
#include "../utils/FlowCalculator.h"
#include "../utils/Log.h"
#include "../utils/exceptions.h"
#include "../io/SafeFile.h"

namespace infomap {

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addStateNode(StateNode node)
{
	auto ret = m_nodes.insert(StateNetwork::NodeMap::value_type(node.id, node));
	if (ret.second) {
		// If state node didn't exist, also create the associated physical node
		addPhysicalNode(node.physicalId);
		if (node.id != node.physicalId) {
			m_haveMemoryInput = true;
		}
	}
	return ret;
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addStateNode(unsigned int id, unsigned int physId)
{
	return addStateNode(StateNode(id, physId));
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addNode(unsigned int id)
{
	return addStateNode(StateNode(id));
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addNode(unsigned int id, double weight)
{
	auto ret = addNode(id);
	auto& node = ret.first->second;
	node.weight = weight;
	return ret;
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addNode(unsigned int id, std::string name)
{
	m_names[id] = name;
	return addNode(id);
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addNode(unsigned int id, std::string name, double weight)
{
	m_names[id] = name;
	return addNode(id, weight);
}

StateNetwork::PhysNode& StateNetwork::addPhysicalNode(unsigned int physId)
{
	auto& physNode = m_physNodes[physId];
	physNode.physId = physId;
	return physNode;
}

StateNetwork::PhysNode& StateNetwork::addPhysicalNode(unsigned int physId, double weight)
{
	auto& physNode = addPhysicalNode(physId);
	physNode.weight = weight;
	return physNode;
}

StateNetwork::PhysNode& StateNetwork::addPhysicalNode(unsigned int physId, const std::string& name)
{
	auto& physNode = addPhysicalNode(physId);
	// physNode.name = name;
	m_names[physId] = name;
	return physNode;
}

StateNetwork::PhysNode& StateNetwork::addPhysicalNode(unsigned int physId, double weight, const std::string& name)
{
	auto& physNode = addPhysicalNode(physId);
	physNode.weight = weight;
	// physNode.name = name;
	m_names[physId] = name;
	return physNode;
}


std::pair<std::map<unsigned int, std::string>::iterator, bool> StateNetwork::addName(unsigned int id, std::string name)
{
	// m_names[id] = name;
	return m_names.insert(std::make_pair(id, name));
}

bool StateNetwork::addLink(unsigned int sourceId, unsigned int targetId, double weight)
{
	if (weight < m_config.weightThreshold) {
		++m_numLinksIgnoredByWeightThreshold;
		m_totalLinkWeightIgnored += weight;
		return false;
	}
	m_totalLinkWeightAdded += weight;

	if (sourceId == targetId) {
		++m_numSelfLinksFound;
		if (!m_config.includeSelfLinks) {
			return false;
		}
		m_sumSelfLinkWeight += weight;
	}
	// const auto& sourceNode = m_nodes.emplace(sourceId, StateNode(sourceId)).first;
	// const auto& targetNode = m_nodes.emplace(targetId, StateNode(targetId)).first;
	// m_nodes.emplace(sourceId, StateNode(sourceId));
	// m_nodes.emplace(targetId, StateNode(targetId));
	addNode(sourceId);
	addNode(targetId);


	++m_numLinks;
	m_sumLinkWeight += weight;

	bool addedNewLink = true;

	// Log() << "Add link " << sourceId << " -> " << targetId << ", weight: " << weight << "\n";
	// Aggregate link weights if they are definied more than once
	auto& outLinks = m_nodeLinkMap[sourceId];
	if (outLinks.empty()) {
		outLinks[targetId] = weight;
		// Log() << "  -> new source link!\n";
	}
	else {
		// auto& ret = outLinks.emplace(targetNode, weight);
		// if (!ret.second)
			// auto& linkData = ret.first;
		// Log() << " " << outLinks.size() << " outlinks exist: ";
		// for (auto& it : outLinks) {
		// 	Log() << "\n  " << it.first.id << ": weight: " << it.second.weight << ", count: " << it.second.count << ".. ";
		// }
		// auto& linkData = outLinks[targetId];
		auto ret = outLinks.insert(std::make_pair(StateNode(targetId), LinkData(weight)));
		// ++linkData.count;
		// Log() << "\n ++count -> weight: " << linkData.weight << ", count: " << linkData.count << ".. ";
		auto& linkData = ret.first->second;
		// if (linkData.count != 1) {
		if (!ret.second) {
			// Log() << "  -> existing weight: " << linkData.weight;
			linkData.weight += weight;
			// Log() << " -> weight: " << linkData.weight << "\n";
			// Log() << "    -> count: " << linkData.count << ", weight: " << linkData.weight << "\n";
			++m_numAggregatedLinks;
			--m_numLinks;
			addedNewLink = false;
		}
		else {
			// Log() << "  -> new target link!\n";
			linkData.weight = weight;
		}
	}
	m_outWeights[sourceId] += weight;
	// printNetwork();
	return addedNewLink;
	// auto& ret = m_nodeLinkMap.emplace(sourceId, std::make_pair(StateNode(), LinkData()));
	// if (!ret.second) {
	// 	ret.first[targetId] = weight;
	// }
	// else {
	// 	auto& ret2 = ret.first.emplace(targetId, weight);
	// 	if (!ret2.second) {
	// 		ret2.first += weight;
	// 		++m_numAggregatedLinks;
	// 		--m_numLinks;
	// 		return false;
	// 	}
	// }
	// return true;

	// LinkMap::iterator firstIt = m_links.lower_bound(sourceId);
	// if (firstIt != m_links.end() && firstIt->first == sourceId) // First linkEnd already exists, check second linkEnd
	// {
	// 	std::pair<std::map<unsigned int, double>::iterator, bool> ret2 = firstIt->second.insert(std::make_pair(targetId, weight));
	// 	if (!ret2.second)
	// 	{
	// 		ret2.first->second += weight;
	// 		++m_numAggregatedLinks;
	// 		--m_numLinks;
	// 		return false;
	// 	}
	// }
	// else
	// {
	// 	m_links.insert(firstIt, std::make_pair(sourceId, std::map<unsigned int, double>()))->second.insert(std::make_pair(targetId, weight));
	// }

	// return true;
}

bool StateNetwork::addPath(const std::vector<unsigned int>& path, unsigned int markovOrder, double weight)
{
	if (markovOrder == 0) {
		throw DataDomainError("Trying to add state nodoes from path with markov order 0, must be 1 or more.");
	}
	if (path.size() <= markovOrder) {
		++m_numSkippedPaths;
		return false;
	}
	unsigned int lastStateId = 0;
	bool createLink = false;
	// std::cout << "Add state node and links from path " << io::stringify(path, " ") << "\n";
	for (unsigned int i = markovOrder - 1; i < path.size(); ++i) {
		std::string id = io::stringifyContainer(path, " ", i - (markovOrder - 1), markovOrder);
		unsigned int physId = path[i];
		unsigned int stateId = physId;
		if (markovOrder > 1) {
			auto ret = m_pathToStateId.insert(std::make_pair(id, m_pathToStateId.size()));
			stateId = ret.first->second;
		}
		addStateNode(stateId, physId);
		// std::cout << " -> add State node '" << id << "' (" << stateId << "," << physId << ")\n";
		if (!createLink) {
			createLink = true;
		} else {
			// std::cout << "   -> add link " << lastStateId << " - " << stateId << " with weight " << weight << "\n";
			addLink(lastStateId, stateId, weight);
		}
		// std::cout << "   => " << m_physNodes.size() << " physical nodes\n";
		lastStateId = stateId;
	}
	return true;
}

void StateNetwork::calculateFlow()
{
	FlowCalculator flowCalculator;
	flowCalculator.calculateFlow(*this, m_config);
}

void StateNetwork::dispose()
{
	m_nodeLinkMap.clear();
	m_physNodes.clear();
	m_nodes.clear();
}

void StateNetwork::writeStateNetwork(std::string filename) const
{
	if (filename.empty())
		throw BadArgumentError("writeStateNetwork called with empty filename");

	SafeOutFile outFile(filename);

	if (!m_names.empty()) {
		outFile << "*Vertices\n";
		for (auto& nameIt : m_names) {
			auto& physId = nameIt.first;
			auto& name = nameIt.second;
			outFile << physId << " \"" << name << "\"\n";
		}
	}

	outFile << "*States\n";
	outFile << "# stateId physicalId\n";
	for (const auto& nodeIt : nodes()) {
		const auto& node = nodeIt.second;
		outFile << node.id << " " << node.physicalId;
		// Optional name
		if (!node.name.empty())
			outFile << " \"" << node.name << "\"";
		outFile << "\n";
	}

	outFile << "*Links\n";
	for (auto& linkIt : m_nodeLinkMap) {
		for (auto& subIt : linkIt.second)
		{
			outFile << linkIt.first.id << " " << subIt.first.id << " " << subIt.second.weight << "\n";
		}
	}
}

void StateNetwork::writePajekNetwork(std::string filename) const
{
	if (filename.empty())
		throw BadArgumentError("writePajekNetwork called with empty filename");

	SafeOutFile outFile(filename);

	outFile << "*Vertices\n";
	for (const auto& nodeIt : nodes()) {
		const auto& node = nodeIt.second;
		outFile << node.id << " \"";
		// Name, default to id
		const auto& nameIt = m_names.find(node.id);
		if (nameIt != m_names.end())
			outFile << nameIt->second;
		else
			outFile << node.id;
		outFile << "\"\n";
	}

	outFile << "*Links\n";
	for (auto& linkIt : m_nodeLinkMap) {
		for (auto& subIt : linkIt.second)
		{
			outFile << linkIt.first.id << " " << subIt.first.id << " " << subIt.second.weight << "\n";
		}
	}
}

std::pair<StateNetwork::NodeMap::iterator, bool> StateNetwork::addStateNodeWithAutogeneratedId(unsigned int physId)
{
	// Keys sorted with std::less comparator, so last key is the largest 
	unsigned int stateId = m_nodes.empty() ? 0 : m_nodes.crbegin()->first + 1;
	return addStateNode(stateId, physId);
}


}