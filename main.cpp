#include <algorithm>
#include <cassert>
#include <filesystem>
#include <numeric>
#include <raylib.h>
#include <unordered_set>
#include <vector>
#include <unordered_map>

const int screen_width = 600;
const int screen_height = 400;
const float aspect_ratio = static_cast<float>(screen_width) / static_cast<float>(screen_height);
const double total_pixels = screen_width * screen_height;

std::vector<Color> colours = {RED, GREEN, BLUE};

double double_check_size = 0;

bool set_contains(std::unordered_set<int>* set, int key)
{
    return set->find(key) != set->end();
}

struct DisplayNode {
    Vector2 dimensions;
    Vector2 positions;
    Color colour;
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
    Treemap& treemap;
};

TreemapNode add_node(Treemap* treemap, std::string name, int parent_index)
{
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

TreemapNode add_directory_node(Treemap* treemap, int parent_index,
    std::filesystem::directory_entry dir_entry)
{
    TreemapNode node = add_node(treemap, dir_entry.path(), parent_index);
    treemap->nodes.at(node.index).size = 0.0;
    return node;
}

TreemapNode add_file_node(Treemap* treemap, int parent_index,
    std::filesystem::directory_entry dir_entry)
{
    TreemapNode node = add_node(treemap, dir_entry.path(), parent_index);
    treemap->leaves.insert(node.index);
    treemap->nodes.at(node.index).size = static_cast<double>(dir_entry.file_size());
    double_check_size += static_cast<double>(dir_entry.file_size());
    return node;
}

void visit_dir(Treemap* treemap, std::filesystem::path path, int parent_index)
{
    for (auto const& dir_entry : std::filesystem::directory_iterator { path }) {
        if (dir_entry.is_directory()) {
            TreemapNode node = add_directory_node(treemap, parent_index, dir_entry);
            visit_dir(treemap, dir_entry.path(), node.index);
        }

        if (dir_entry.is_regular_file()) {
            TreemapNode node = add_file_node(treemap, parent_index, dir_entry);
        }
    }
};

void init_treemap_data(Treemap* treemap)
{
    std::string string_path = "../../steele";
    printf("Visiting path: %s\n", string_path.c_str());
    // Add the root
    TreemapNode root_node = add_directory_node(
        treemap, -1,
        std::filesystem::directory_entry(std::filesystem::path(string_path)));
    visit_dir(treemap, std::filesystem::path(string_path), root_node.index);
}

void print_treemap(Treemap* treemap, int current_index)
{
    /*
    if (treemap->leaves.find(current_index) != treemap->leaves.end()) {
        printf(
            "Leaf detected at index: [%d], from %s of size %.1f\n",
            current_index,
            treemap->nodes.at(current_index).name.c_str(),
            treemap->nodes.at(current_index).size
        );
    }
    if (!set_contains(&treemap->leaves, current_index)) {250000Aspect
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

void populate_parent_size(Treemap* treemap, int current_index)
{
    TreemapNode* current_node = &treemap->nodes.at(current_index);
    for (int child_idx : current_node->children_indices) {
        if (!set_contains(&treemap->leaves, child_idx)) {
            populate_parent_size(treemap, child_idx);
        }
        current_node->size += treemap->nodes.at(child_idx).size;
    }
}

struct DisplayCell {
    Vector2 position;
    Vector2 dimensions;
    float aspect_ratio;
    double size;
    Color color;
};

struct DisplayBoundingBox {
    Vector2 position;
    Vector2 dimensions;
    std::vector<int> display_cells;
    bool row_oriented;
    double empty_area;
    double occupied_area;
    double area;
};

double worst(std::vector<double> *areas, float width) {
    double s = std::accumulate(areas->begin(), areas->end(), 0);
    double worst_aspect_ratio = -1;
    for (double area: *areas) {
        double t0 = (width * width * area) / (s * s);
        double t1 = (s * s) / (width * width * area);
        worst_aspect_ratio = std::max(std::max(t0, t1), worst_aspect_ratio);
    }
    return worst_aspect_ratio;
}

struct LayoutNode {
    Vector2 position;
    Vector2 dimensions;
    std::vector<double> values;
    double area;
};

struct Layout {
    int current_index;
    std::unordered_map<int, int> parent_map;
    std::vector<LayoutNode> layout_nodes;
    std::vector<DisplayNode> display_nodes;
    double pixel_scale_factor;
};

double get_width(LayoutNode *layout_node) {
    return  layout_node->position.x 
            ? layout_node->position.x < layout_node->position.y 
            : layout_node->position.y;
}

void layout_row(Layout *layout, std::vector<double> *current_row) {
    LayoutNode current_layout_node = layout->layout_nodes.at(layout->current_index);
    double row_area = std::accumulate(current_row->begin(), current_row->end(), 0);
    std::vector<DisplayNode> display_nodes = {};
    
    // A layout node is column oriented if it is wider than it is tall.
    // The column_oriented boolean determines in which direction to pack
    // the rectangles.
    bool column_oriented = (current_layout_node.dimensions.x > current_layout_node.dimensions.y);

    for (double size: *current_row) {
        DisplayNode display_node;
        if (column_oriented) {
            double scale_factor =  size / row_area;
            display_node.dimensions.y   = current_layout_node.dimensions.y * scale_factor;
            display_node.dimensions.x   = size * layout->pixel_scale_factor 
                                        / current_layout_node.dimensions.y;
        }
        else {}
        display_nodes.push_back(display_node);
    }
    
    // Handle the positioning of the boxes within the layout node.
    Vector2 current_position = current_layout_node.position;
    for (int i = 0 ; i < display_nodes.size(); i++) {
        DisplayNode *display_node = &display_nodes.at(i);
        display_node->positions = current_layout_node.position;
        display_node->colour = colours.at(i % colours.size());
        current_position.x += (!column_oriented)*display_node->dimensions.x;
        current_position.y += column_oriented*display_node->dimensions.y;
        layout->display_nodes.push_back(*display_node);
    }

    layout->current_index ++;
    layout->layout_nodes.push_back({});
    LayoutNode *child_layout_node = &layout->layout_nodes.at(layout->current_index);
}

void squarify(Layout *layout, int index, std::vector<double> *sizes, std::vector<double> *current_row, float width) {
    double current_child = sizes->at(index);
    double aspect_ratio_before = worst(current_row, width);
    current_row->push_back(current_child);
    double aspect_ratio_after = worst(current_row, width);
    current_row->pop_back();
    if (aspect_ratio_before <= aspect_ratio_after) {
        current_row->push_back(current_child);
        if (index == sizes->size() - 1) return;
        squarify(layout, index++, sizes, current_row, width);
    }
    else {
        layout_row(layout, current_row);
        squarify(layout, index, sizes, {}, get_width(&layout->layout_nodes.at(layout->current_index)));
    }
}

void populate_layout(Layout *layout, std::vector<double> *sizes) {
    // Set up the root layout to be the size of the screen.
    LayoutNode root_layout_node;
    root_layout_node.position = {0,0};
    root_layout_node.dimensions = {screen_width, screen_height};
    root_layout_node.area = std::accumulate(sizes->begin(), sizes->end(), 0);

    layout->current_index = 0;
    layout->layout_nodes.push_back(root_layout_node);
    layout->pixel_scale_factor = (root_layout_node.dimensions.x * root_layout_node.dimensions.y) / root_layout_node.area;
}

void draw_treemap(Treemap *treemap, std::vector<double> *sizes) {
    Layout layout = {};
    populate_layout(&layout, sizes);
    squarify(&layout, 0, sizes, {}, get_width(&layout.layout_nodes.at(layout.current_index)));

    BeginDrawing();
        ClearBackground(RAYWHITE);
        for (const DisplayNode display_node: layout.display_nodes) {
            DrawRectangle(
                display_node.positions.x, 
                display_node.positions.y, 
                display_node.dimensions.x, 
                display_node.dimensions.y, 
                display_node.colour
            );
        }
    EndDrawing();
}

void compute_sizes(Treemap *treemap) { populate_parent_size(treemap, 0); }

int main()
{
    Treemap treemap = {};
    init_treemap_data(&treemap);
    compute_sizes(&treemap);
    InitWindow(screen_width, screen_height, "raylib");
    SetTargetFPS(60);
    std::vector<double> sizes = {6.0, 6.0, 4.0, 3.0, 2.0, 2.0, 1.0};
    while (!WindowShouldClose()) {
        draw_treemap(&treemap, &sizes);
    }
    /*
    print_treemap(&treemap, 0);
    float disparity = static_cast<double>(treemap.nodes.at(0).size) - double_check_size;
    printf("root size: %f\n", treemap.nodes.at(0).size);
    printf("double_check_size: %1.f\n", double_check_size);
    printf("Total disparity: %1.f \n", disparity);
    */
    return 0;
}
