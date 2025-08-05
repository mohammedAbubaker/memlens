#include <cassert>
#include <filesystem>
#include <raylib.h>
#include <unordered_set>
#include <vector>

double double_check_size = 0;

bool set_contains(std::unordered_set<int> *set, int key) {
  return set->find(key) != set->end();
}

struct DisplayNode {
  int treemap_node_index;
};

struct TreemapNode {
  std::string name;
  double size;
  int parent_index;
  std::vector<int> children_indices;
  int index;
};

struct Treemap {
  std::vector<TreemapNode> nodes;
  std::unordered_set<int> leaves;
};

struct TreemapDisplay {
  std::vector<DisplayNode> nodes;
  Treemap &treemap;
};

TreemapNode add_node(Treemap *treemap, std::string name, int parent_index) {
  TreemapNode node = {};
  if (!treemap->nodes.size()) {
    assert(parent_index == -1);
  } else {
    assert(parent_index != -1);
  }

  int index = treemap->nodes.size();
  node.index = index;
  node.name = name;
  node.parent_index = parent_index;
  treemap->nodes.push_back(node);
  if (parent_index != -1) {
    treemap->nodes.at(parent_index).children_indices.push_back(node.index);
  }
  return node;
}

TreemapNode add_directory_node(Treemap *treemap, int parent_index,
                               std::filesystem::directory_entry dir_entry) {
  TreemapNode node = add_node(treemap, dir_entry.path(), parent_index);
  treemap->nodes.at(node.index).size = 0.0;
  return node;
}

TreemapNode add_file_node(Treemap *treemap, int parent_index,
                          std::filesystem::directory_entry dir_entry) {
  TreemapNode node = add_node(treemap, dir_entry.path(), parent_index);
  treemap->leaves.insert(node.index);
  treemap->nodes.at(node.index).size =
      static_cast<double>(dir_entry.file_size());
  double_check_size += static_cast<double>(dir_entry.file_size());
  return node;
}

void visit_dir(Treemap *treemap, std::filesystem::path path, int parent_index) {
  for (auto const &dir_entry : std::filesystem::directory_iterator{path}) {
    if (dir_entry.is_directory()) {
      TreemapNode node = add_directory_node(treemap, parent_index, dir_entry);
      visit_dir(treemap, dir_entry.path(), node.index);
    }

    if (dir_entry.is_regular_file()) {
      TreemapNode node = add_file_node(treemap, parent_index, dir_entry);
    }
  }
};

void init_treemap_data(Treemap *treemap) {
  std::string string_path = "../../steele";
  printf("Visiting path: %s\n", string_path.c_str());
  // Add the root
  TreemapNode root_node = add_directory_node(
      treemap, -1,
      std::filesystem::directory_entry(std::filesystem::path(string_path)));
  visit_dir(treemap, std::filesystem::path(string_path), root_node.index);
}

void print_treemap(Treemap *treemap, int current_index) {
  /*
  if (treemap->leaves.find(current_index) != treemap->leaves.end()) {
      printf(
          "Leaf detected at index: [%d], from %s of size %.1f\n",
          current_index,
          treemap->nodes.at(current_index).name.c_str(),
          treemap->nodes.at(current_index).size
      );
  }
  if (!set_contains(&treemap->leaves, current_index)) {
      printf(
          "Directory detected at index: [%d], from %s of size %.1f\n",
          current_index,
          treemap->nodes.at(current_index).name.c_str(),
          treemap->nodes.at(current_index).size
      );
  }
  */
  for (int child_index : treemap->nodes.at(current_index).children_indices) {
    print_treemap(treemap, child_index);
  }
}

void populate_parent_size(Treemap *treemap, int current_index) {
  TreemapNode *current_node = &treemap->nodes.at(current_index);
  for (int child_idx : current_node->children_indices) {
    if (!set_contains(&treemap->leaves, child_idx)) {
      populate_parent_size(treemap, child_idx);
    }
    current_node->size += treemap->nodes.at(child_idx).size;
  }
}

void compute_sizes(Treemap *treemap) { populate_parent_size(treemap, 0); }

int main() {
  Treemap treemap = {};
  init_treemap_data(&treemap);
  compute_sizes(&treemap);
  print_treemap(&treemap, 0);
  float disparity =
      static_cast<double>(treemap.nodes.at(0).size) - double_check_size;
  printf("root size: %f\n", treemap.nodes.at(0).size);
  printf("double_check_size: %1.f\n", double_check_size);
  printf("Total disparity: %1.f \n", disparity);
  return 0;
}
