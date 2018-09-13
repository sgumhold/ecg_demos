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

void polygon_view::after_insert_loop(size_t loop_idx)
{
	if (find_control(loop_index))
		find_control(loop_index)->set("max", poly.nr_loops() - 1);

	if (loop_index >= loop_idx) {
		++loop_index;
		update_member(&loop_index);
	}
}

void polygon_view::on_change_loop(size_t loop_idx, int flags)
{
	if (loop_idx != loop_index)
		return;
	if ((flags & PLA_BEGIN) != 0) {
		current_loop.first_vertex = poly.loop_begin(loop_idx);
		update_member(&current_loop.first_vertex);
	}
	if ((flags & PLA_SIZE) != 0) {
		current_loop.nr_vertices = poly.loop_size(loop_idx);
		update_member(&current_loop.nr_vertices);
	}
	if ((flags & PLA_CLOSED) != 0) {
		current_loop.is_closed = poly.loop_closed(loop_idx);
		update_member(&current_loop.is_closed);
	}
	if ((flags & PLA_COLOR) != 0) {
		current_loop.color = poly.loop_color(loop_idx);
		update_member(&current_loop.color);
	}
	if ((flags & PLA_ORIENTATION) != 0) {
		current_loop.orientation = poly.loop_orientation(loop_idx);
		update_member(&current_loop.orientation);
	}
}

void polygon_view::before_remove_loop(size_t loop_idx)
{
	// before removal of last loop
	if (poly.nr_loops() == 1) {
		loop_index = 0;
		update_member(&loop_index);
		if (find_control(loop_index))
			find_control(loop_index)->set("max", 0);
		return;
	}
	if (find_control(loop_index))
		find_control(loop_index)->set("max", poly.nr_loops() - 2);

	if (loop_idx > loop_index)
		return;

	if (loop_idx < loop_index) {
		--loop_index;
		update_member(&loop_index);
		return;
	}
	// same loop index removed as currently in gui

	// if it is the first loop, set to second loop
	if (loop_idx == 0) {
		loop_index = 1;
		on_set(&loop_index);
		loop_index = 0;
		update_member(&loop_index);
		return;
	}
	// otherwise select 0 th loop
	loop_index = 0;
	on_set(&loop_index);
}

void polygon_view::after_insert_vertex(size_t vtx_idx)
{
	if (vtx_idx <= vertex_index) {
		++vertex_index;
		update_member(&vertex_index);
	}
	if (find_control(vertex_index))
		find_control(vertex_index)->set("max", poly.nr_vertices() - 1);
}

void polygon_view::on_change_vertex(size_t vtx_idx)
{
	if (vtx_idx != vertex_index)
		return;
	current_vertex = poly.vertex(vtx_idx);
	update_member(&current_vertex[0]);
	update_member(&current_vertex[1]);
}

void polygon_view::before_remove_vertex(size_t vtx_idx)
{
	if (poly.nr_vertices() == 1) {
		if (find_control(vertex_index))
			find_control(vertex_index)->set("max", 0);
		vertex_index = 0;
		update_member(&vertex_index);
		return;
	}
	if (find_control(vertex_index))
		find_control(vertex_index)->set("max", poly.nr_vertices() - 2);

	if (vtx_idx > vertex_index)
		return;
	if (vtx_idx < vertex_index) {
		--vertex_index;
		update_member(&vertex_index);
		return;
	}
	if (vtx_idx == 0) {
		vertex_index = 1;
		on_set(&vertex_index);
		vertex_index = 0;
		update_member(&vertex_index);
		return;
	}
	vertex_index = 0;
	on_set(&vertex_index);
}

void polygon_view::before_remove_vertex_range(size_t vtx_begin, size_t vtx_end)
{
	if (poly.nr_vertices() == vtx_end - vtx_begin) {
		if (find_control(vertex_index))
			find_control(vertex_index)->set("max", 0);
		vertex_index = 0;
		update_member(&vertex_index);
		return;
	}
	if (find_control(vertex_index))
		find_control(vertex_index)->set("max", poly.nr_vertices() - vtx_end + vtx_begin - 1);

	if (vtx_begin > vertex_index)
		return;
	if (vtx_end <= vertex_index) {
		vertex_index -= vtx_end - vtx_begin;
		update_member(&vertex_index);
		return;
	}
	if (vtx_begin == 0) {
		vertex_index = vtx_end;
		on_set(&vertex_index);
		vertex_index = 0;
		update_member(&vertex_index);
		return;
	}
	vertex_index = 0;
	on_set(&vertex_index);
}

void polygon_view::on_new_polygon()
{
	vertex_colors.resize(poly.nr_vertices());
	loop_index = 0;
	vertex_index = 0;
	if (find_control(loop_index)) {
		find_control(loop_index)->set("max", poly.nr_loops() - 1);
		find_control(vertex_index)->set("max", poly.nr_vertices() - 1);
	}
	on_set(&loop_index);
	on_set(&vertex_index);

	std::fill(vertex_colors.begin(), vertex_colors.end(), clr_type(128, 128, 128));
	post_redraw();
}

/// find closest polygon vertex to p that is less than max_dist appart
size_t polygon_view::find_closest_vertex(const vtx_type& p, float max_dist) const
{
	size_t vtx_idx = size_t(-1); //TODO: UINT_MAX
	float min_dist = 0;
	for (size_t vi = 0; vi < poly.nr_vertices(); ++vi) {
		float dist = (poly.vertex(vi) - p).length();
		if (dist <= max_dist) 
			if (vtx_idx == size_t(-1) || dist < min_dist) {
				min_dist = dist;
				vtx_idx = vi;
			}
	}
	return vtx_idx;
}

/// find closest polygon edge to p that is less than max_dist appart and set edge_point to closest point on found edge
size_t polygon_view::find_closest_edge(const vtx_type& p, float max_dist, vtx_type& edge_point) const
{
	size_t edge_insert_vtx_index = size_t(-1); //TODO: UINT_MAX
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

			// compute edge lambda and check that it is in [0,1] // TODO: [0,1] or (0,1) ??
			vtx_type d = p1 - p0;
			float lambda = dot(p - p0, d) / dot(d, d);
			if (lambda > 0 && lambda < 1) {

				// compute projected point
				vtx_type projected_point = p0 + lambda * d;
				float dist = (projected_point - p).length();

				// check distance
				if (dist < max_dist) {
					if (edge_insert_vtx_index == size_t(-1) /* TODO: UINT_MAX */ || dist < min_dist) {
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

polygon_view::polygon_view() : cgv::base::group("polygon_view"), current_loop(0, 1)
{
	rasterizer = new polygon_rasterizer(poly);

	background_color = clr_type(255, 255, 128);

	if (!poly.read(QUOTE_SYMBOL_VALUE(INPUT_DIR) "/poly.txt"))
		poly.generate_circle(8);
	else
		poly.center_and_scale_to_unit_box();


	connect(poly.after_insert_loop, this, &polygon_view::after_insert_loop);
	connect(poly.on_change_loop, this, &polygon_view::on_change_loop);
	connect(poly.before_remove_loop, this, &polygon_view::before_remove_loop);
	connect(poly.after_insert_vertex, this, &polygon_view::after_insert_vertex);
	connect(poly.on_change_vertex, this, &polygon_view::on_change_vertex);
	connect(poly.before_remove_vertex, this, &polygon_view::before_remove_vertex);
	connect(poly.before_remove_vertex_range, this, &polygon_view::before_remove_vertex_range);

	on_new_polygon();

	pnt_render_style.illumination_mode = IM_OFF;
	pnt_render_style.culling_mode = CM_OFF;
	pnt_render_style.orient_splats = false;
	pnt_render_style.point_size = 16;
	view_ptr = 0;
	selected_index = size_t(-1); //TODO: UINT_MAX
	edge_insert_vtx_index = size_t(-1); //TODO: UINT_MAX

	loop_index = 0;
	vertex_index = 0;
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

void polygon_view::draw_polygon() //TODO: rasterizer switch
{
	for (size_t li = 0; li < poly.nr_loops(); ++li) {
		if (poly.loop_size(li) < 2)
			continue;
		GLenum gl_type = poly.loop_closed(li) ? GL_LINE_LOOP : GL_LINE_STRIP;
		glColor3ubv(&poly.loop_color(li)[0]);
		glBegin(gl_type);
		for (size_t vi = poly.loop_begin(li); vi<poly.loop_end(li); ++vi)
			glVertex2fv(poly.vertex(vi));
		glEnd();
	}
}


void polygon_view::draw_vertices(context& ctx)
{
	clr_type tmp(255, 0, 255);
	if (selected_index != size_t(-1)) // TODO: UINT_MAX
		std::swap(tmp, vertex_colors[selected_index]);

	pnt_renderer.set_color_array(ctx, vertex_colors);
	pnt_renderer.set_position_array(ctx, &poly.vertex(0), poly.nr_vertices());
	pnt_renderer.validate_and_enable(ctx);
	glDrawArrays(GL_POINTS, 0, poly.nr_vertices());
	pnt_renderer.disable(ctx);

	if (selected_index != size_t(-1)) // TODO: UINT_MAX
		std::swap(tmp, vertex_colors[selected_index]);


	if (edge_insert_vtx_index != size_t(-1)) { // TODO: UINT_MAX
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
	if (rasterizer->handle(e))
		return true;

	if (e.get_kind() == EID_KEY) {
		key_event& ke = static_cast<key_event&>(e);
		if (ke.get_action() == KA_RELEASE)
			return false;
		switch (ke.get_key()) {
		case 'W':
			poly.write(QUOTE_SYMBOL_VALUE(INPUT_DIR) "/my_poly.txt");
			break;
		case 'R':
			poly.clear();
			poly.read(QUOTE_SYMBOL_VALUE(INPUT_DIR) "/my_poly.txt");
			on_new_polygon();
			break;
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
						else if ((edge_point - old_edge_point).length() > 0.5f * max_dist / pnt_render_style.point_size)
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
						case cgv::gui::EM_CTRL: //TODO: are the brackets needed here?
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
					if (selected_index != size_t(-1)) { // TODO: are the brackets needed here?
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
	if (poly.nr_vertices() > 0 && member_ptr >= &poly.vertex(0) && member_ptr < &poly.vertex(0) + poly.nr_vertices())
		rasterizer->rasterize_polygon();

	if (member_ptr == &loop_index) {
		current_loop.color = poly.loop_color(loop_index);
		current_loop.first_vertex = poly.loop_begin(loop_index);
		current_loop.nr_vertices = poly.loop_size(loop_index);
		current_loop.orientation = poly.loop_orientation(loop_index);
		current_loop.is_closed = poly.loop_closed(loop_index);
		update_member(&current_loop.color);
		update_member(&current_loop.first_vertex);
		update_member(&current_loop.nr_vertices);
		update_member(&current_loop.orientation);
		update_member(&current_loop.is_closed);
	}
	if (member_ptr == &current_loop.color) 
		poly.set_loop_color(loop_index, current_loop.color);
	if (member_ptr == &current_loop.is_closed && current_loop.is_closed != poly.loop_closed(loop_index)) {
		if (current_loop.is_closed)
			poly.close_loop(loop_index);
		else
			poly.open_loop(loop_index);
	}
	if (member_ptr == &current_loop.color)
		poly.set_loop_color(loop_index, current_loop.color);
	if (member_ptr >= &current_vertex && member_ptr < &current_vertex + 1) {
		poly.set_vertex(vertex_index, current_vertex);
		rasterizer->rasterize_polygon();
	}
	if (member_ptr == &selected_index) {
		if (selected_index != size_t(-1) && vertex_index != selected_index) {
			vertex_index = selected_index;
			on_set(&vertex_index);
			size_t loop_idx = poly.find_loop(selected_index);
			if (loop_idx != loop_index) {
				loop_index = loop_idx;
				on_set(&loop_index);
			}
		}
	}
	if (member_ptr == &vertex_index) {
		current_vertex = poly.vertex(vertex_index);
		update_member(&current_vertex[0]);
		update_member(&current_vertex[1]);
	}

	update_member(member_ptr);
	post_redraw();
}

void polygon_view::create_gui()
{
	add_decorator("polygon view", "heading");

	inline_object_gui(rasterizer);

	if (begin_tree_node("rendering", pnt_render_style)) {
		align("\a");
		add_member_control(this, "background_color", background_color);
		add_gui("point style", pnt_render_style);
		align("\b");
		end_tree_node(pnt_render_style);
	}

	if (begin_tree_node("polygon", poly)) {
		align("\a");
		add_member_control(this, "loop_index", loop_index, "value_slider", "min=0;max=1;ticks=true");
		find_control(loop_index)->set("max", poly.nr_loops() - 1);
		align("\a");
		add_member_control(this, "loop color", current_loop.color);
		add_member_control(this, "loop closed", current_loop.is_closed);
		add_view("loop size", current_loop.nr_vertices);
		add_view("loop begin", current_loop.first_vertex);
		add_member_control(this, "loop orientation", current_loop.orientation, "dropdown", "enums='undef,ccw,cw'");
		align("\b");
		add_member_control(this, "vertex_index", vertex_index, "value_slider", "min=0;max=1;ticks=true");
		find_control(vertex_index)->set("max", poly.nr_vertices() - 1);
		align("\a");
		add_gui("vertex", current_vertex, "", "options='min=-2;max=2;ticks=true'");
		align("\b");
		align("\b");
		end_tree_node(poly);
	}
}


#include <cgv/base/register.h>

extern factory_registration<polygon_view> pv_fac("new/polygon_view", 'P', true);

