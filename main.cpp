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

// ----- gedcom node -----
struct GedcomNode {
    int level;
    std::string xref_id;
    std::string tag;
    std::string value;
    std::vector<std::shared_ptr<GedcomNode>> children;
};

// ----- trim whitespace -----
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    size_t last = str.find_last_not_of(" \t\r\n");
    return (first == std::string::npos) ? "" : str.substr(first, last - first + 1);
}

// ----- parse line -----
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

// ----- parse gedcom file into node tree -----
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

        if (!xref.empty()) {
            xrefMap[xref] = node;
        }

        while (!nodeStack.empty() && nodeStack.top()->level >= level) {
            nodeStack.pop();
        }

        if (nodeStack.empty()) {
            roots.push_back(node);
        } else {
            nodeStack.top()->children.push_back(node);
        }

        nodeStack.push(node);
    }

    return roots;
}

// ----- event structure -----
struct Event {
    std::string type;
    std::string date;
    std::string place;
};

// ----- individual structure -----
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

// ----- family structure -----
struct Family {
    std::string id;
    std::string husband_id;
    std::string wife_id;
    std::vector<std::string> children_ids;
    std::vector<Event> events;
    std::vector<std::string> notes;
    std::vector<std::string> sources;
};

// ----- note and source structure -----
std::map<std::string, std::string> notesMap;
std::map<std::string, std::string> sourcesMap;

// ----- extract event from gedcom node -----
Event parseEvent(const std::shared_ptr<GedcomNode>& node) {
    Event event;
    event.type = node->tag;
    for (const auto& child : node->children) {
        if (child->tag == "DATE") event.date = child->value;
        if (child->tag == "PLAC") event.place = child->value;
    }
    return event;
}

// ----- extract notes and sources -----
void extractNotesAndSources(const std::unordered_map<std::string, std::shared_ptr<GedcomNode>>& xrefMap) {
    for (const auto& [xref, node] : xrefMap) {
        if (node->tag == "NOTE") {
            std::string text = node->value;
            for (const auto& c : node->children) {
                if (c->tag == "CONT" || c->tag == "CONC") text += "\n" + c->value;
            }
            notesMap[xref] = text;
        }
        if (node->tag == "SOUR") {
            sourcesMap[xref] = node->value;
        }
    }
}

// ----- extract individuals -----
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

// ----- extract families -----
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

// ----- print data to console -----
void printData(const std::unordered_map<std::string, Individual>& individuals,
               const std::unordered_map<std::string, Family>& families) {
    std::cout << "individuals:\n";
    for (const auto& [id, indi] : individuals) {
        std::cout << "id: " << id << ", name: " << indi.name << ", sex: " << indi.sex << "\n";
        for (const auto& e : indi.events) {
            std::cout << "  event: " << e.type << ", date: " << e.date << ", place: " << e.place << "\n";
        }
    }

    std::cout << "\nfamilies:\n";
    for (const auto& [id, fam] : families) {
        std::cout << "id: " << id << ", husband: " << fam.husband_id << ", wife: " << fam.wife_id << "\n";
        for (const auto& c : fam.children_ids) {
            std::cout << "  child: " << c << "\n";
        }
    }
}

// ----- export to json -----
void exportToJson(const std::unordered_map<std::string, Individual>& individuals,
                  const std::unordered_map<std::string, Family>& families) {
    std::ofstream out("ctree_output.json");
    out << "{\n  \"individuals\": [\n";
    bool first = true;
    for (const auto& [id, indi] : individuals) {
        if (!first) out << ",\n";
        first = false;
        out << "    {\n";
        out << "      \"id\": \"" << id << "\",\n";
        out << "      \"name\": \"" << indi.name << "\",\n";
        out << "      \"sex\": \"" << indi.sex << "\",\n";
        out << "      \"events\": [";
        for (size_t i = 0; i < indi.events.size(); ++i) {
            const auto& e = indi.events[i];
            if (i > 0) out << ", ";
            out << "{ \"type\": \"" << e.type << "\", \"date\": \"" << e.date << "\", \"place\": \"" << e.place << "\" }";
        }
        out << "]\n    }";
    }
    out << "\n  ],\n  \"families\": [\n";
    first = true;
    for (const auto& [id, fam] : families) {
        if (!first) out << ",\n";
        first = false;
        out << "    {\n";
        out << "      \"id\": \"" << id << "\",\n";
        out << "      \"husband\": \"" << fam.husband_id << "\",\n";
        out << "      \"wife\": \"" << fam.wife_id << "\",\n";
        out << "      \"children\": [";
        for (size_t i = 0; i < fam.children_ids.size(); ++i) {
            if (i > 0) out << ", ";
            out << "\"" << fam.children_ids[i] << "\"";
        }
        out << "]\n    }";
    }
    out << "\n  ]\n}\n";
    out.close();
}

// ----- main -----
int main() {
    std::string filename = "example.ged";
    std::unordered_map<std::string, std::shared_ptr<GedcomNode>> xrefMap;

    auto gedcomTree = parseGedcom(filename, xrefMap);
    extractNotesAndSources(xrefMap);
    auto individuals = extractIndividuals(xrefMap);
    auto families = extractFamilies(xrefMap);

    printData(individuals, families);
    exportToJson(individuals, families);

    std::cout << "\ndata exported to ctree_output.json\n";
    return 0;
}
