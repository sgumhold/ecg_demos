#pragma once

#include "polygon.h"
#include "polygon_rasterizer.h"
#include <cgv/base/group.h>
#include <cgv/gui/event_handler.h>
#include <cgv/gui/provider.h>
#include <cgv/render/drawable.h>
#include <cgv/render/view.h>
#include <cgv_gl/point_renderer.h>

/// class to interact with polygons
class polygon_view :
	public cgv::base::group,          /// derive from node to integrate into global tree structure and to store a name
	public polygon_types,
	public cgv::gui::event_handler,  /// derive from handler to receive events and to be asked for a help string
	public cgv::gui::provider,
	public cgv::render::drawable     /// derive from drawable for drawing the cube
{
protected:
	// store pointer to view that renders this drawable
	cgv::render::view* view_ptr;

	// rendering members
	clr_type background_color;
	float line_width;
	cgv::render::point_render_style pnt_render_style;
	cgv::render::point_renderer pnt_renderer;

	// interaction members
private:
	size_t selected_index;
	size_t edge_insert_vtx_index;
	vtx_type last_pos;
	vtx_type edge_point;

	size_t vertex_index;
	size_t loop_index;
	vtx_type current_vertex;
	polygon_loop current_loop;

	// polygon members
protected:
	polygon poly;
	std::vector<clr_type> vertex_colors;

	// managed objects
	cgv::data::ref_ptr<polygon_rasterizer> rasterizer;

	void on_new_polygon();

	/// callbacks used to update gui
	void after_insert_loop(size_t loop_idx);
	void on_change_loop(size_t loop_idx, int flags);
	void before_remove_loop(size_t loop_idx);
	void after_insert_vertex(size_t vtx_idx);
	void on_change_vertex(size_t vtx_idx);
	void before_remove_vertex(size_t vtx_idx);
	void before_remove_vertex_range(size_t vtx_begin, size_t vtx_end);

	/// find closest polygon vertex to p that is less than max_dist appart
	size_t find_closest_vertex(const vtx_type& p, float max_dist) const;
	/// find closest polygon edge to p that is less than max_dist appart; return vertex index where edge point needs to be inserted and set edge_point to closest point on found edge
	size_t find_closest_edge(const vtx_type& p, float max_dist, vtx_type& edge_point) const;
public:

	/// object management functions
	polygon_view();
	///
	std::string get_type_name() const { return "polygon_view"; }
	/// process changes to member variables
	void on_set(void* member_ptr);

	// rendering functions
	bool init(cgv::render::context& ctx);
	void init_frame(cgv::render::context& ctx);
	void draw_polygon();
	void draw_vertices(cgv::render::context& ctx);
	void draw(cgv::render::context& ctx);
	void stream_help(std::ostream& os);
	void clear(cgv::render::context& ctx);

	// gui stuff
	bool handle(cgv::gui::event& e);
	std::string get_menu_path() const { return "demo/polygon"; }
	void create_gui();
};
