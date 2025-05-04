#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <stack>
#include <unordered_map>
#include <algorithm>
#include <map>

struct GedcomNode {
    int level;
    std::string xref_id;
    std::string tag;
    std::string value;
    std::vector<std::shared_ptr<GedcomNode>> children;
};

struct Event {
    std::string type;
    std::string date;
    std::string place;
};

struct Individual {
    std::string id;
    std::string name;
    std::string sex;
    std::vector<Event> events;
    std::string famc;
    std::vector<std::string> fams;
    std::vector<std::string> notes;
    std::vector<std::string> sources;
};

struct Family {
    std::string id;
    std::string husband_id;
    std::string wife_id;
    std::vector<std::string> children_ids;
    std::vector<Event> events;
    std::vector<std::string> notes;
    std::vector<std::string> sources;
};

std::map<std::string, std::string> notesMap;
std::map<std::string, std::string> sourcesMap;

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    size_t last = str.find_last_not_of(" \t\r\n");
    return (first == std::string::npos) ? "" : str.substr(first, last - first + 1);
}

bool parseLine(const std::string& line, int& level, std::string& xref, std::string& tag, std::string& value) {
    std::istringstream iss(line);
    if (!(iss >> level)) return false;
    std::string token1, token2;
    if (!(iss >> token1)) return false;

    if (token1[0] == '@' && token1.back() == '@') {
        xref = token1;
        if (!(iss >> tag)) return false;
    } else {
        xref = "";
        tag = token1;
    }
    std::getline(iss, value);
    value = trim(value);
    return true;
}

std::vector<std::shared_ptr<GedcomNode>> parseGedcom(const std::string& filename, std::unordered_map<std::string, std::shared_ptr<GedcomNode>>& xrefMap) {
    std::ifstream file(filename);
    std::string line;
    std::vector<std::shared_ptr<GedcomNode>> roots;
    std::stack<std::shared_ptr<GedcomNode>> nodeStack;

    while (std::getline(file, line)) {
        int level;
        std::string xref, tag, value;
        if (!parseLine(line, level, xref, tag, value)) continue;

        auto node = std::make_shared<GedcomNode>();
        node->level = level;
        node->xref_id = xref;
        node->tag = tag;
        node->value = value;

        if (!xref.empty()) xrefMap[xref] = node;

        while (!nodeStack.empty() && nodeStack.top()->level >= level) nodeStack.pop();

        if (nodeStack.empty()) roots.push_back(node);
        else nodeStack.top()->children.push_back(node);

        nodeStack.push(node);
    }

    return roots;
}

Event parseEvent(const std::shared_ptr<GedcomNode>& node) {
    Event event;
    event.type = node->tag;
    for (const auto& child : node->children) {
        if (child->tag == "DATE") event.date = child->value;
        if (child->tag == "PLAC") event.place = child->value;
    }
    return event;
}

void extractNotesAndSources(const std::unordered_map<std::string, std::shared_ptr<GedcomNode>>& xrefMap) {
    for (const auto& [xref, node] : xrefMap) {
        if (node->tag == "NOTE") {
            std::string text = node->value;
            for (const auto& c : node->children)
                if (c->tag == "CONT" || c->tag == "CONC") text += "\n" + c->value;
            notesMap[xref] = text;
        } else if (node->tag == "SOUR") {
            sourcesMap[xref] = node->value;
        }
    }
}

std::unordered_map<std::string, Individual> extractIndividuals(const std::unordered_map<std::string, std::shared_ptr<GedcomNode>>& xrefMap) {
    std::unordered_map<std::string, Individual> individuals;
    for (const auto& [xref, node] : xrefMap) {
        if (node->tag != "INDI") continue;
        Individual indi;
        indi.id = xref;
        for (const auto& child : node->children) {
            if (child->tag == "NAME") indi.name = child->value;
            else if (child->tag == "SEX") indi.sex = child->value;
            else if (child->tag == "BIRT" || child->tag == "DEAT") indi.events.push_back(parseEvent(child));
            else if (child->tag == "FAMC") indi.famc = child->value;
            else if (child->tag == "FAMS") indi.fams.push_back(child->value);
            else if (child->tag == "NOTE") indi.notes.push_back(child->value);
            else if (child->tag == "SOUR") indi.sources.push_back(child->value);
        }
        individuals[xref] = indi;
    }
    return individuals;
}

std::unordered_map<std::string, Family> extractFamilies(const std::unordered_map<std::string, std::shared_ptr<GedcomNode>>& xrefMap) {
    std::unordered_map<std::string, Family> families;
    for (const auto& [xref, node] : xrefMap) {
        if (node->tag != "FAM") continue;
        Family fam;
        fam.id = xref;
        for (const auto& child : node->children) {
            if (child->tag == "HUSB") fam.husband_id = child->value;
            else if (child->tag == "WIFE") fam.wife_id = child->value;
            else if (child->tag == "CHIL") fam.children_ids.push_back(child->value);
            else if (child->tag == "MARR") fam.events.push_back(parseEvent(child));
            else if (child->tag == "NOTE") fam.notes.push_back(child->value);
            else if (child->tag == "SOUR") fam.sources.push_back(child->value);
        }
        families[xref] = fam;
    }
    return families;
}

void displayIndividuals(const std::unordered_map<std::string, Individual>& individuals,
                        const std::map<std::string, std::string>& simplifiedIds) {
    for (const auto& [id, indi] : individuals) {
        std::cout << "ID: " << simplifiedIds.at(id) << " | Name: " << indi.name << " | Sex: " << indi.sex << "\n";
        for (const auto& e : indi.events) {
            std::cout << "  Event: " << e.type << " | Date: " << e.date << " | Place: " << e.place << "\n";
        }
        for (const auto& sid : indi.sources) {
            if (sourcesMap.count(sid))
                std::cout << "  Source: " << sourcesMap[sid] << "\n";
            else
                std::cout << "  Source: " << sid << " (unresolved)\n";
        }
        for (const auto& nid : indi.notes) {
            if (notesMap.count(nid))
                std::cout << "  Note: " << notesMap[nid] << "\n";
            else
                std::cout << "  Note: " << nid << " (unresolved)\n";
        }
        std::cout << "\n";
    }
}

void displayFamilies(const std::unordered_map<std::string, Family>& families,
                     const std::map<std::string, std::string>& simplifiedIds) {
    for (const auto& [id, fam] : families) {
        std::cout << "Family ID: " << id << "\n";
        std::cout << "  Husband: " << (simplifiedIds.count(fam.husband_id) ? simplifiedIds.at(fam.husband_id) : fam.husband_id) << "\n";
        std::cout << "  Wife: " << (simplifiedIds.count(fam.wife_id) ? simplifiedIds.at(fam.wife_id) : fam.wife_id) << "\n";
        for (const auto& child : fam.children_ids) {
            std::cout << "  Child: " << (simplifiedIds.count(child) ? simplifiedIds.at(child) : child) << "\n";
        }
        std::cout << "\n";
    }
}

void showAncestors(const std::string& id,
                   const std::unordered_map<std::string, Individual>& individuals,
                   const std::unordered_map<std::string, Family>& families,
                   const std::map<std::string, std::string>& simplifiedIds,
                   int depth = 0) {
    auto it = individuals.find(id);
    if (it == individuals.end()) return;
    const Individual& indi = it->second;

    for (int i = 0; i < depth; ++i) std::cout << "  ";
    std::cout << "+- " << indi.name << " (" << simplifiedIds.at(id) << ")\n";

    auto famIt = families.find(indi.famc);
    if (famIt != families.end()) {
        const Family& fam = famIt->second;
        if (!fam.husband_id.empty()) showAncestors(fam.husband_id, individuals, families, simplifiedIds, depth + 1);
        if (!fam.wife_id.empty()) showAncestors(fam.wife_id, individuals, families, simplifiedIds, depth + 1);
    }
}

int main() {
    std::string filename = "example.ged";
    std::unordered_map<std::string, std::shared_ptr<GedcomNode>> xrefMap;
    auto gedcomTree = parseGedcom(filename, xrefMap);
    extractNotesAndSources(xrefMap);
    auto individuals = extractIndividuals(xrefMap);
    auto families = extractFamilies(xrefMap);

    std::map<std::string, std::string> simplifiedIds;
    std::map<std::string, std::string> simplifiedToOriginal;
    int counter = 1;
    for (const auto& [id, _] : individuals) {
        std::string simpleId = "IND" + std::to_string(counter++);
        simplifiedIds[id] = simpleId;
        simplifiedToOriginal[simpleId] = id;
    }

    while (true) {
        std::cout << "\n===== GEDCOM Console Menu =====" << std::endl;
        std::cout << "1. View Individuals" << std::endl;
        std::cout << "2. View Families" << std::endl;
        std::cout << "3. Exit" << std::endl;
        std::cout << "4. Show Ancestors of an Individual" << std::endl;
        std::cout << "Choose an option: ";

        int choice;
        std::cin >> choice;
        std::cin.ignore();

        if (choice == 1) {
            displayIndividuals(individuals, simplifiedIds);
        } else if (choice == 2) {
            displayFamilies(families, simplifiedIds);
        } else if (choice == 3) {
            break;
        } else if (choice == 4) {
            std::string inputId;
            std::cout << "Enter individual ID (e.g. IND1): ";
            std::getline(std::cin, inputId);

            if (simplifiedToOriginal.count(inputId)) {
                std::string originalId = simplifiedToOriginal[inputId];
                showAncestors(originalId, individuals, families, simplifiedIds);
            } else {
                std::cout << "Invalid ID. Please use a valid simplified ID like IND1.\n";
            }
        } else {
            std::cout << "Invalid choice. Try again.\n";
        }
    }

    return 0;
}
