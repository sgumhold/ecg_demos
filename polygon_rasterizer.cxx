#include "polygon_rasterizer.h"
#include <cgv/gui/key_event.h>
#include <cgv_gl/gl/gl.h>

bool polygon_rasterizer::validate_pixel_location(const pixel_type& p) const
{
	return p(0) >= 0 && p(0) < int(img_width) && p(1) >= 0 && p(1) < int(img_height);
}

size_t polygon_rasterizer::linear_index(const pixel_type& p) const
{
	return img_width * p(1) + p(0);
}

polygon_rasterizer::pixel_type polygon_rasterizer::round(const vtx_type& p)
{
	return pixel_type(int(floor(p(0) + 0.5f)), int(floor(p(1) + 0.5f)));
}

void polygon_rasterizer::set_pixel(const pixel_type& p, const clr_type& c)
{
	if (validate_pixel_location(p))
		img[linear_index(p)] = c;
}

const polygon_rasterizer::clr_type& polygon_rasterizer::get_pixel(const pixel_type& p) const
{
	return img[linear_index(p)];
}

polygon_rasterizer::vtx_type polygon_rasterizer::pixel_from_world(const vtx_type& p) const
{
	return vtx_type(float(img_width), float(img_height))*(p - img_extent.get_min_pnt()) / img_extent.get_extent();
}

polygon_rasterizer::vtx_type polygon_rasterizer::world_from_pixel(const vtx_type& p) const
{
	return p * img_extent.get_extent() / vtx_type(float(img_width), float(img_height)) + img_extent.get_min_pnt();
}

void polygon_rasterizer::clear_image()
{
	for (size_t y = 0; y<img_height; ++y)
		for (size_t x = 0; x<img_width; ++x)
			img[linear_index(pixel_type(x, y))] = bg_clr[(x + y) & 1];
}

void polygon_rasterizer::rasterize_polygon()
{
	//clear the canvas
	clear_image();
	// iterate all the vertices
	for (size_t vi = 1; vi < poly.nr_vertices(); ++vi) {
		// propose, that poly.vertex(vi)(0) poly.vertex(vi)(1) is x and y.
		// use Bresenham algorythm
		pixel_type pix_0 = pixel_from_world(poly.vertex(vi-1));
		pixel_type pix_1 = pixel_from_world(poly.vertex(vi));
		int x0 = pix_0(0);
		int y0 = pix_0(1);
		int x1 = pix_1(0);
		int y1 = pix_1(1);

		const bool steep = (abs(y1 - y0) > abs(x1 - x0));
		if (steep) {
			std::swap(x0, y0);
			std::swap(x1, y1);
		}

		if (x0 > x1) {
			std::swap(x0, x1);
			std::swap(y0, y1);
		}
		const int dx = x1 - x0;
		const int dy = abs(y1 - y0);

		int error = dx / 2;
		const int ystep = (y0 < y1) ? 1 : -1;
		int y = y0;

		const int maxX = x1;

		for (int x = x0; x < maxX; x++) {
			vtx_type tempVec;
			if (steep)
				set_pixel(pixel_type(y, x));
			else 
				set_pixel(pixel_type(x, y));
			error -= dy;
			if (error < 0) {
				y += ystep;
				error += dx;
			}
		}
	}
	tex_outofdate = true;
}

void polygon_rasterizer::reallocate_image()
{
	img.resize(img_width*img_height);
	clear_image();
	tex_outofdate = true;
}

polygon_rasterizer::polygon_rasterizer(const polygon& _poly) : node("polygon_rasterizer"), poly(_poly)
{
	bg_clr[0] = clr_type(255, 230, 230);
	bg_clr[1] = clr_type(230, 255, 230);
	fg_clr = clr_type(255, 0, 0);
	tex.set_mag_filter(cgv::render::TF_NEAREST);
	img_width = img_height = 64;
	img_extent.ref_min_pnt() = vtx_type(-2, -2);
	img_extent.ref_max_pnt() = vtx_type(2, 2);
	synch_img_dimensions = true;
	reallocate_image();
	tex_outofdate = true;
}

/// return name of type
std::string polygon_rasterizer::get_type_name() const
{
	return "polygon_rasterizer";
}

void polygon_rasterizer::init_frame(cgv::render::context& ctx)
{
	if (tex_outofdate) {
		if (tex.is_created())
			tex.destruct(ctx);
		cgv::data::data_format df("uint8[R,G,B]");
		df.set_width(img_width);
		df.set_height(img_height);
		cgv::data::data_view dv(&df, &img[0]);
		tex.create(ctx, dv);
		tex_outofdate = false;
	}
}

void polygon_rasterizer::clear(cgv::render::context& ctx)
{
	tex.destruct(ctx);
}

void polygon_rasterizer::on_set(void* member_ptr)
{
	if (member_ptr == &img_height || member_ptr == &img_width) {
		if (synch_img_dimensions) {
			if (member_ptr == &img_width) {
				img_height = img_width;
				update_member(&img_height);
			}
			else {
				img_width = img_height;
				update_member(&img_width);
			}
		}
		reallocate_image();
		rasterize_polygon();
		tex_outofdate = true;
	}
	update_member(member_ptr);
	post_redraw();
}


void polygon_rasterizer::draw(cgv::render::context& ctx)
{
	glPushMatrix();
	glScaled(2, 2, 2);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	tex.enable(ctx);
	ctx.tesselate_unit_square();
	tex.disable(ctx);
	glPopMatrix();
}

/// add
bool polygon_rasterizer::handle(cgv::gui::event& e)
{
	if (e.get_kind() == cgv::gui::EID_KEY) {
		cgv::gui::key_event& ke = static_cast<cgv::gui::key_event&>(e);
		if (ke.get_action() == cgv::gui::KA_RELEASE)
			return false;
		switch (ke.get_key()) {
		case 'S':
			synch_img_dimensions = !synch_img_dimensions;
			on_set(&synch_img_dimensions);
			return true;
		}
	}
	return false;
}

///
void polygon_rasterizer::stream_help(std::ostream& os)
{
	os << "toggle <s>ynch.img.dims";
}

/// you must overload this for gui creation
void polygon_rasterizer::create_gui()
{
	bool show_tree = begin_tree_node("rasterization", synch_img_dimensions, false, "align=' ';options='w=132'");
	add_member_control(this, "show", cgv::render::drawable::active, "toggle", "w=60");
	if (show_tree) {
		align("\a");
		add_member_control(this, "synch_img_dimensions", synch_img_dimensions, "toggle");
		add_member_control(this, "img_width", img_width, "value_slider", "min=2;max=1024;log=true;ticks=true");
		add_member_control(this, "img_height", img_height, "value_slider", "min=2;max=1024;log=true;ticks=true");
		add_member_control(this, "bg_color0", bg_clr[0]);
		add_member_control(this, "bg_color1", bg_clr[1]);
		add_member_control(this, "fg_color", fg_clr);
		align("\b");
		end_tree_node(synch_img_dimensions);
	}
}
