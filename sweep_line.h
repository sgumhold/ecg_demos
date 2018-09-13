#pragma once

#include "polygon.h"
#include <cgv/base/node.h>
#include <cgv/gui/event_handler.h>
#include <cgv/gui/provider.h>
#include <cgv/render/drawable.h>
#include <cgv/render/texture.h>


class sweep_line :
	public cgv::base::node,          /// derive from node to integrate into global tree structure and to store a name
	public cgv::gui::event_handler,  /// derive from handler to receive events and to be asked for a help string
	public polygon_types,
	public cgv::gui::provider,
	public cgv::render::drawable     /// derive from drawable for drawing the cube
{
public:
	typedef cgv::math::fvec<int, 2> pixel_type;
private:
	bool tex_outofdate;
  unsigned int line_pos;
protected:
	const polygon& poly;
	clr_type bg_clr[2];
	clr_type fg_clr;
	cgv::render::texture tex;
	std::vector<clr_type> img;
	size_t img_width, img_height;
	box_type img_extent;
	bool synch_img_dimensions;
	bool validate_pixel_location(const pixel_type& p) const;
	size_t linear_index(const pixel_type& p) const;
	static pixel_type round(const vtx_type& p);
	void set_pixel(const pixel_type& p, const clr_type& c);
	const clr_type& get_pixel(const pixel_type& p) const;
	vtx_type pixel_from_world(const vtx_type& p) const;
	vtx_type world_from_pixel(const vtx_type& p) const;
	void clear_image();
	void reallocate_image();
public:
	sweep_line(const polygon& _poly);
	void rasterize_polygon();
	///
	void on_set(void* member_ptr);
	/// return name of type
	std::string get_type_name() const;
	void init_frame(cgv::render::context& ctx);
	/// free texture
	void clear(cgv::render::context& ctx);
	/// draw the image as a texture
	void draw(cgv::render::context& ctx);
	/// add
	bool handle(cgv::gui::event& e);
	///
	void stream_help(std::ostream& os);
	/// you must overload this for gui creation
	void create_gui();
};
