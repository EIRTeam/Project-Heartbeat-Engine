
#ifndef KV_TABLE_H
#define KV_TABLE_H

#include "core/error/error_macros.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/variant/variant.h"

class KVTable {
	struct KVTableNode {
		String value;
		StringName path;
		StringName name;
		Vector<StringName> children;
	};
	LocalVector<KVTableNode> nodes;
	HashMap<StringName, int> key_values;

public:
	void parse(String p_text) {
		PackedStringArray lines = p_text.split("\n");
		for (const String &line : lines) {
			const String line_stripped = line.strip_edges();
			if (line_stripped.begins_with("#")) {
				continue;
			}
			PackedStringArray parts = line_stripped.split("=");
			if (parts.size() == 2) {
				PackedStringArray property_parts = parts[0].split(".");
				String property_accumulator;
				KVTableNode *table_node = nullptr;
				for (const String &property_part : property_parts) {
					if (property_accumulator.is_empty()) {
						property_accumulator += property_part;
					} else {
						property_accumulator += "." + property_part;
					}
					StringName property_accum_name = StringName(property_accumulator);
					HashMap<StringName, int>::Iterator it = key_values.find(property_accum_name);
					if (it == key_values.end()) {
						// No existing node in this path, create it
						KVTableNode node{
							.path = property_accum_name,
							.name = StringName(property_part),
						};
						if (table_node) {
							// Add this node to parent
							StringName parent_path = table_node->path;
							nodes.push_back(node);
							nodes[key_values[parent_path]].children.push_back(property_accum_name);
						} else {
							// Node is a root node, just add it
							nodes.push_back(node);
						}
						key_values.insert(property_accum_name, nodes.size() - 1);
						table_node = &nodes[nodes.size() - 1];
					} else {
						// Existing node in this path, store it and continue moving forward
						table_node = &nodes[it->value];
					}
				}
				if (table_node) {
					table_node->value = parts[1];
				}
			}
		}
	}
	int get_children_count(const StringName &p_path) const {
		HashMap<StringName, int>::ConstIterator it = key_values.find(p_path);
		Vector<const KVTableNode *> children;
		ERR_FAIL_COND_V(it == key_values.end(), 0);
		int i = it->value;
		return nodes[i].children.size();
	}

	bool has_key(const StringName &p_path) const {
		HashMap<StringName, int>::ConstIterator it = key_values.find(p_path);
		return it != key_values.end();
	}

	StringName child_get_path(const StringName &p_path, int p_child, const StringName &p_key) const {
		HashMap<StringName, int>::ConstIterator it = key_values.find(p_path);
		ERR_FAIL_COND_V(it == key_values.end(), "");
		int i = it->value;
		ERR_FAIL_INDEX_V(p_child, nodes[i].children.size(), "");

		StringName child_path = nodes[i].children[p_child];
		int child_i = key_values[child_path];
		for (StringName child : nodes[child_i].children) {
			if (nodes[key_values[child]].name == p_key) {
				return nodes[key_values[child]].path;
			}
		}
		return "";
	}

	bool child_has_key(const StringName &p_path, int p_child, const StringName &p_key) const {
		HashMap<StringName, int>::ConstIterator it = key_values.find(p_path);
		ERR_FAIL_COND_V(it == key_values.end(), false);
		int i = it->value;
		ERR_FAIL_INDEX_V(p_child, nodes[i].children.size(), false);

		StringName child_path = nodes[i].children[p_child];
		int child_i = key_values[child_path];
		for (StringName child : nodes[child_i].children) {
			if (nodes[key_values[child]].name == p_key) {
				return true;
			}
		}
		return false;
	}

	String child_get_value(const StringName &p_path, int p_child, const StringName &p_key) {
		HashMap<StringName, int>::ConstIterator it = key_values.find(p_path);
		ERR_FAIL_COND_V(it == key_values.end(), "");
		int i = it->value;
		ERR_FAIL_INDEX_V(p_child, nodes[i].children.size(), "");

		StringName child_path = nodes[i].children[p_child];
		int child_i = key_values[child_path];
		for (StringName child : nodes[child_i].children) {
			if (nodes[key_values[child]].name == p_key) {
				return nodes[key_values[child]].value;
			}
		}
		return "";
	}

	String get_value(const StringName &p_path) const {
		HashMap<StringName, int>::ConstIterator it = key_values.find(p_path);
		ERR_FAIL_COND_V_MSG(it == key_values.end(), "", vformat("Key %s not found", p_path));
		int i = it->value;
		ERR_FAIL_COND_V(nodes[i].children.size() != 0, "");
		return nodes[i].value;
	}
};

#endif // KV_TABLE_H
