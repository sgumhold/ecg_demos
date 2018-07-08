#include "polygon_view.h"
#include <cgv/defines/quote.h>
#include <cgv/signal/rebind.h>
#include <cgv/base/find_action.h>
#include <cgv/gui/key_event.h>
#include <cgv/gui/mouse_event.h>
#include <cgv_gl/gl/gl.h>
#include <fstream>

using namespace cgv::base;
using namespace cgv::signal;
using namespace cgv::gui;
using namespace cgv::render;
using namespace cgv::utils;
using namespace cgv::math;

void polygon_view::on_new_polygon()
{
	vertex_colors.resize(poly.nr_vertices());
	std::fill(vertex_colors.begin(), vertex_colors.end(), clr_type(128, 128, 128));
}

/// find closest polygon vertex to p that is less than max_dist appart
size_t polygon_view::find_closest_vertex(const vtx_type& p, float max_dist) const
{
	size_t vtx_idx = size_t(-1);
	float min_dist = 0;
	for (size_t vi = 0; vi < poly.nr_vertices(); ++vi) {
		float dist = (poly.vertex(vi) - p).length();
		if (dist <= max_dist) {
			if (vtx_idx == size_t(-1) || dist < min_dist) {
				min_dist = dist;
				vtx_idx = vi;
			}
		}
	}
	return vtx_idx;
}

/// find closest polygon edge to p that is less than max_dist appart and set edge_point to closest point on found edge
size_t polygon_view::find_closest_edge(const vtx_type& p, float max_dist, vtx_type& edge_point) const
{
	size_t edge_insert_vtx_index = size_t(-1);
	float min_dist = 0;

	// iterate all edges
	for (size_t li = 0; li < poly.nr_loops(); ++li) {
		size_t vbegin = poly.loop_begin(li);
		size_t vi_last = poly.loop_end(li) - 1;
		if (!poly.loop_closed(li)) {
			vi_last = vbegin;
			++vbegin;
		}
		for (size_t vi = vbegin; vi < poly.loop_end(li); ++vi) {
			// for each edge extract vertex locations
			const vtx_type& p0 = poly.vertex(vi_last);
			const vtx_type& p1 = poly.vertex(vi);

			// compute edge lambda and check that it is in [0,1]
			vtx_type d = p1 - p0;
			float lambda = dot(p - p0, d) / dot(d, d);
			if (lambda > 0 && lambda < 1) {

				// compute projected point
				vtx_type projected_point = p0 + lambda*d;
				float dist = (projected_point - p).length();

				// check distance
				if (dist < max_dist) {
					if (edge_insert_vtx_index == size_t(-1) || dist < min_dist) {
						min_dist = dist;
						edge_insert_vtx_index = vi;
						edge_point = projected_point;
					}
				}
			}
			vi_last = vi;
		}
	}
	return edge_insert_vtx_index;
}

polygon_view::polygon_view() : cgv::base::group("polygon_view")
{
	rasterizer = new polygon_rasterizer(poly);

	background_color = clr_type(255, 255, 128);

	if (!poly.read(QUOTE_SYMBOL_VALUE(INPUT_DIR) "/poly.txt"))
		poly.generate_circle(8);
	else
		poly.center_and_scale_to_unit_box();

	on_new_polygon();

	pnt_render_style.illumination_mode = IM_OFF;
	pnt_render_style.culling_mode = CM_OFF;
	pnt_render_style.orient_splats = false;
	pnt_render_style.point_size = 16;
	view_ptr = 0;
	selected_index = size_t(-1);
	edge_insert_vtx_index = size_t(-1);
}

void polygon_view::stream_help(std::ostream& os)
{
}

bool polygon_view::init(context& ctx)
{
	ctx.configure_new_child(rasterizer);
	std::vector<cgv::render::view*> views;
	cgv::base::find_interface<cgv::render::view>(cgv::base::base_ptr(this), views);
	if (views.empty())
		return false;
	view_ptr = views[0];
	if (pnt_renderer.init(ctx)) {
		pnt_renderer.set_reference_point_size(0.005f);
		pnt_renderer.set_y_view_angle(float(view_ptr->get_y_view_angle()));
		pnt_renderer.set_render_style(pnt_render_style);
		return true;
	}
	return false;
}

void polygon_view::clear(context& ctx)
{
	pnt_renderer.clear(ctx);
}

void polygon_view::draw_polygon()
{
	for (size_t li = 0; li < poly.nr_loops(); ++li) {
		if (poly.loop_size(li) < 2)
			continue;
		GLenum gl_type = poly.loop_closed(li) ? GL_LINE_LOOP : GL_LINE_STRIP;
		glColor3ubv(&poly.loop_color(li)[0]);
		glBegin(gl_type);
		for (size_t vi=poly.loop_begin(li); vi<poly.loop_end(li); ++vi)
			glVertex2fv(poly.vertex(vi));
		glEnd();
	}
}


void polygon_view::draw_vertices(context& ctx)
{
	clr_type tmp(255,0,255);
	if (selected_index != size_t(-1))
		std::swap(tmp, vertex_colors[selected_index]);

	pnt_renderer.set_color_array(ctx, vertex_colors);
	pnt_renderer.set_position_array(ctx, &poly.vertex(0), poly.nr_vertices());
	pnt_renderer.validate_and_enable(ctx);
	glDrawArrays(GL_POINTS, 0, poly.nr_vertices());
	pnt_renderer.disable(ctx);

	if (selected_index != size_t(-1))
		std::swap(tmp, vertex_colors[selected_index]);


	if (edge_insert_vtx_index != size_t(-1)) {
		pnt_renderer.set_color_array(ctx, &tmp, 1);
		pnt_renderer.set_position_array(ctx, &edge_point, 1);
		pnt_renderer.validate_and_enable(ctx);
		glDrawArrays(GL_POINTS, 0, 1);
		pnt_renderer.disable(ctx);

	}
}

void polygon_view::init_frame(context& ctx)
{
	rasterizer->init_frame(ctx);
}

void polygon_view::draw(context& ctx)
{
	draw_vertices(ctx);
	glLineWidth(5);
	glColor3f(0.8f, 0.5f, 0);
	draw_polygon();

	if (rasterizer->is_visible())
		rasterizer->draw(ctx);
	else {
		glColor3ubv(&background_color[0]);
		glPushMatrix();
		glScaled(2, 2, 2);
		ctx.tesselate_unit_square();
		glPopMatrix();
	}
}

bool polygon_view::handle(event& e)
{
	if (e.get_kind() == EID_KEY) {
		key_event& ke = static_cast<key_event&>(e);
		if (ke.get_action() == KA_RELEASE)
			return false;
		switch (ke.get_key()) {
		default: break;
		}
	}
	else if (e.get_kind() == EID_MOUSE) {
		mouse_event& me = static_cast<mouse_event&>(e);
		switch (me.get_action()) {
		case MA_MOVE:
			if (view_ptr) {
				float max_dist = (float)(pnt_render_style.point_size * view_ptr->get_y_extent_at_focus() / get_context()->get_height());
				cgv::math::fvec<double, 3> p_d;
				if (get_world_location(me.get_x(), me.get_y(), *view_ptr, p_d)) {
					vtx_type p = vtx_type(float(p_d(0)), float(p_d(1)));
					size_t new_selected_index = find_closest_vertex(p, max_dist);
					if (new_selected_index != selected_index) {
						selected_index = new_selected_index;
						on_set(&selected_index);
					}
					// if no point found, check for edges
					if (selected_index == size_t(-1)) {
						vtx_type old_edge_point = edge_point;
						size_t new_edge_insert_vtx_index = find_closest_edge(p, max_dist, edge_point);
						if (new_edge_insert_vtx_index != edge_insert_vtx_index) {
							edge_insert_vtx_index = new_edge_insert_vtx_index;
							on_set(&edge_insert_vtx_index);
						}
						else if ((edge_point - old_edge_point).length() > 0.5f*max_dist / pnt_render_style.point_size)
							on_set(&edge_point);
					}
					else {
						if (edge_insert_vtx_index != size_t(-1)) {
							edge_insert_vtx_index = size_t(-1);
							on_set(&edge_insert_vtx_index);
						}
					}
				}
			}
			break;
		case MA_DRAG:
			if (me.get_button_state() == MB_LEFT_BUTTON) {
				if (selected_index != size_t(-1)) {
					cgv::math::fvec<double, 3> p_d;
					if (get_world_location(me.get_x(), me.get_y(), *view_ptr, p_d)) {
						vtx_type new_pos = vtx_type(float(p_d(0)), float(p_d(1)));
						vtx_type diff = new_pos - last_pos;
						last_pos = new_pos;
						switch (me.get_modifiers()) {
						case 0:
							poly.set_vertex(selected_index, poly.vertex(selected_index) + diff);
							on_set(const_cast<vtx_type*>(&poly.vertex(selected_index)));
							return true;
						case cgv::gui::EM_CTRL:
						{
							size_t loop_idx = poly.find_loop(selected_index);
							for (size_t vi = poly.loop_begin(loop_idx); vi < poly.loop_end(loop_idx); ++vi) {
								poly.set_vertex(vi, poly.vertex(vi) + diff, false);
								on_set(const_cast<vtx_type*>(&poly.vertex(selected_index)));
							}
							return true;
						}
						}

					}
				}
				return false;
			}
			break;
		case MA_PRESS:
			if (me.get_button() == MB_LEFT_BUTTON) {
				// last position defaults to edge point 
				if (edge_insert_vtx_index != size_t(-1))
					last_pos = edge_point;
				// try to compute world location below mouse pointer
				cgv::math::fvec<double, 3> p_d;
				if (get_world_location(me.get_x(), me.get_y(), *view_ptr, p_d)) {
					last_pos = vtx_type(float(p_d(0)), float(p_d(1)));
					// if vertex is selected, nothing to be done
					if (selected_index != size_t(-1)) {
					}
					// if edge point selected, insert vertex on edge
					else if (edge_insert_vtx_index != size_t(-1)) {
						edge_point = last_pos;
						poly.insert_vertex(last_pos, edge_insert_vtx_index);
						vertex_colors.insert(vertex_colors.begin() + edge_insert_vtx_index, clr_type(128, 128, 128));
						selected_index = edge_insert_vtx_index;
						edge_insert_vtx_index = size_t(-1);
						on_set(const_cast<vtx_type*>(&poly.vertex(selected_index)));
						on_set(&edge_insert_vtx_index);
						on_set(&selected_index);
					}
					// if nothing was selected create or extend new loop
					else {
						if (poly.nr_loops() == 0 || poly.loop_size(poly.nr_loops() - 1) > 2) {
							size_t vtx_idx = poly.append_loop(last_pos);
							if (vtx_idx == vertex_colors.size())
								vertex_colors.push_back(clr_type(128, 128, 128));
							else
								vertex_colors.insert(vertex_colors.begin() + vtx_idx, clr_type(128, 128, 128));
							selected_index = vtx_idx;
							on_set(&selected_index);
							on_set(const_cast<vtx_type*>(&poly.vertex(vtx_idx)));
						}
						else {
							size_t loop_idx = poly.nr_loops() - 1;
							size_t vtx_idx = poly.append_vertex_to_loop(last_pos, loop_idx);
							if (vtx_idx == vertex_colors.size())
								vertex_colors.push_back(clr_type(128, 128, 128));
							else
								vertex_colors.insert(vertex_colors.begin() + vtx_idx, clr_type(128, 128, 128));
							if (poly.loop_size(loop_idx) == 3)
								poly.close_loop(loop_idx);

							selected_index = vtx_idx;
							on_set(&selected_index);
							on_set(const_cast<vtx_type*>(&poly.vertex(vtx_idx)));
						}
					}
				}
				return true;
			}
			else if (me.get_button() == MB_RIGHT_BUTTON) {
				if (selected_index != size_t(-1)) {
					poly.remove_vertex(selected_index);
					vertex_colors.erase(vertex_colors.begin() + selected_index);
					selected_index = size_t(-1);
					on_set(&selected_index);
					return true;
				}
			}
			break;
		case MA_RELEASE:
			break;
		}
	}
	return false;
}

void polygon_view::on_set(void* member_ptr)
{
	if (member_ptr >= &poly.vertex(0) && member_ptr < &poly.vertex(0)+poly.nr_vertices()) {
		rasterizer->rasterize_polygon();
	}
	update_member(member_ptr);
	post_redraw();
}

void polygon_view::on_register()
{
//	cgv::base::group::append_child(rasterizer);
//	cgv::base::register_object(rasterizer);
}


void polygon_view::create_gui() 
{	
	add_decorator("polygon view", "heading");

	inline_object_gui(rasterizer);
	//add_member_control(this, "nr_steps", nr_steps, "value_slider", "min=0;max=100;ticks=true");
	//find_control(nr_steps)->set("max", polygon.size() - 2);
	//add_member_control(this, "lambda", lambda, "value_slider", "min=0;max=0.5;ticks=true");
	//add_member_control(this, "wireframe", wireframe, "check");
	/*if (begin_tree_node("rasterization", synch_img_dimensions)) {
		align("\a");
		add_member_control(this, "synch_img_dimensions", synch_img_dimensions, "toggle");
		add_member_control(this, "img_width", img_width, "value_slider", "min=2;max=1024;log=true;ticks=true");
		add_member_control(this, "img_height", img_height, "value_slider", "min=2;max=1024;log=true;ticks=true");
		add_member_control(this, "background_color", background_color);
		align("\b");
		end_tree_node(synch_img_dimensions);
	}*/
	if (begin_tree_node("rendering", pnt_render_style)) {
		align("\a");
		add_member_control(this, "background_color", background_color);
		add_gui("point style", pnt_render_style);
		align("\b");
		end_tree_node(pnt_render_style);
	}
}


#include <cgv/base/register.h>

extern factory_registration<polygon_view> pv_fac("new/polygon_view", 'P', true);

