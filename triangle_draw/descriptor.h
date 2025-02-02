#pragma once
struct GraphicsContext;
extern int setup_descriptor_set_layout(struct GraphicsContext* graphics_context);
extern int setup_descriptors(struct GraphicsContext* graphics_context);
extern int destroy_descriptors(struct GraphicsContext* graphics_context);