#include <iostream>
#include <NeneNode.hpp>

// NeneNode
NeneNode::NeneNode(std::string s)
    : name(std::move(s)) {}

void NeneNode::make_tree() {
    init_node();
    for (auto &kv : children) {
        if (kv.second) kv.second->make_tree();
    }
}

void NeneNode::pulse_sdl_event(SDL_Event* ev) {
    if (!valve_opening_sdl_event) return;
    handle_sdl_event(ev);
    for (auto &kv : children) {
        if (kv.second) kv.second->pulse_sdl_event(ev);
    }
}

void NeneNode::pulse_time_lapse(float dt) {
    if (!valve_opening_sdl_event) return;
    handle_time_lapse(dt);
    for (auto &kv : children) {
        if (kv.second) kv.second->pulse_time_lapse(dt);
    }
}





void NeneLeaf::add_child(std::unique_ptr<NeneNode>)
{
    std::cerr << "！["+this->name+"] ねねリーフには子を追加できません。\n";
}