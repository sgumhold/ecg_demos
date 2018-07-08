#include "polygon.h"
#include <fstream>
#include <sstream>

/// constuct loop 
polygon_loop::polygon_loop(size_t _fst_vtx, size_t _nr_vts, clr_type _clr, bool _is_clsd) :
	orientation(PO_UNDEF), first_vertex(_fst_vtx), nr_vertices(_nr_vts), color(_clr), is_closed(_is_clsd) {
}

/// assert that loop index is within valid range
void polygon::validate_loop_index(size_t loop_idx) const 
{
	assert(loop_idx < loops.size()); 
}

/// assert that vertex index is within valid range
void polygon::validate_vertex_index(size_t vtx_idx) const 
{
	assert(vtx_idx < vertices.size()); 
}

///  function to compute the orientation of a loop
PolygonOrientation polygon::compute_orientation(size_t loop_idx) const
{
	if (!loop_closed(loop_idx))
		return PO_UNDEF;
	float cp_sum = 0;
	size_t last_vi = loop_end(loop_idx)-1;
	for (size_t vi = loop_begin(loop_idx); vi < loop_end(loop_idx); ++vi) {
		const vtx_type& p0 = vertex(last_vi);
		const vtx_type& p1 = vertex(vi);
		cp_sum += p0(0)*p1(1) - p0(1)*p1(0);
		last_vi = vi;
	}
	return cp_sum < 0 ? PO_CW : PO_CCW;
}

/// construct empty polygon
polygon::polygon() 
{
}

/// remove all loops and all vertices
void polygon::clear()
{
	if (nr_vertices() > 0) {
		before_remove_vertex_range(0, nr_vertices());
		vertices.clear();
	}
	if (nr_loops() > 0) {
		for (size_t li = nr_loops(); li > 0; --li)
			before_remove_loop(li-1);
		loops.clear();
	}
}

/// compute axis aligned bounding box of vertices
polygon::box_type polygon::compute_box() const
{
	box_type box;
	box.invalidate();
	for (size_t vi = 0; vi < nr_vertices(); ++vi)
		box.add_point(vertex(vi));
	return box;
}

/// center and scale polygon into box [-1,1]^2
void polygon::center_and_scale_to_unit_box()
{
	box_type box = compute_box();
	float scale = 2.0f / box.get_extent()[box.get_max_extent_coord_index()];
	vtx_type ctr = box.get_center();
	for (size_t vi = 0; vi < nr_vertices(); ++vi) {
		vertices[vi] = scale*(vertices[vi] - ctr);
		on_change_vertex(vi);
	}
}

void polygon::generate_circle(size_t nr_vts)
{
	append_loop(vtx_type(1, 0));
	for (size_t vi = 1; vi < nr_vts; ++vi) {
		float angle = float(2 * M_PI*vi / nr_vts);
		append_vertex_to_loop(vtx_type(cos(angle), sin(angle)));
	}
}

/// read polygon from text file
bool polygon::read(const std::string& file_name)
{
	std::ifstream is(file_name.c_str());
	if (is.fail())
		return false;
	char buffer[1025];
	buffer[1024] = 0;
	float x, y;

	// read first line to determine whether we have a file with loops
	is.getline(buffer, 1024);
	std::string str(buffer);
	std::stringstream ss(str);
	ss >> x >> y;

	if (!ss.fail()) {
		append_loop(vtx_type(x, y));
		while (!is.eof()) {
			is >> x >> y;
			if (is.fail())
				break;
			append_vertex_to_loop(vtx_type(x, y));
		}
		close_loop(0);
	}
	else {
		ss.str() = str;
		size_t nr_loops;
		ss >> nr_loops;
		if (ss.fail())
			return false;

		for (size_t li = 0; li < nr_loops; ++li) {
			if (is.eof())
				return false;
			is.getline(buffer, 1024);
			str = std::string(buffer);
			ss.str() = str;
			size_t nr_vertices;
			int closed, r, g, b;
			ss >> nr_vertices >> closed;
			if (ss.fail())
				return false;
			ss >> r >> g >> b;
			if (ss.fail()) {
				r = 0;
				b = 0;
				g = 0;
			}
			if (nr_vertices == 0)
				return false;

			is.getline(buffer, 1024);
			str = std::string(buffer);
			ss.str() = str;

			ss >> x >> y;
			if (is.fail())
				return false;

			append_loop(vtx_type(x, y));			
			set_loop_color(li, clr_type(r, g, b));

			for (size_t vi = 1; vi < nr_vertices; ++vi) {
				is.getline(buffer, 1024);
				str = std::string(buffer);
				ss.str() = str;

				ss >> x >> y;
				if (is.fail())
					return false;

				size_t vtx_idx = append_vertex_to_loop(vtx_type(x, y));
			}
			if (closed != 0)
				close_loop(li);
		}
	}
	return true;
}

/// write polygon to text file
bool polygon::write(const std::string& file_name) const
{
	if (nr_loops() == 0)
		return false;

	std::ofstream os(file_name);
	if (os.fail())
		return false;

	// check for simple format
	if (nr_loops() == 1 && loop_closed(0) && int(loop_color(0).R()) != 0 && int(loop_color(0).B()) != 0 && int(loop_color(0).G()) != 0) {
		for (size_t vi = 0; vi < nr_vertices(); ++vi)
			os << vertex(vi) << std::endl;
		return true;
	}

	// format including loops
	os << nr_loops() << std::endl;
	for (size_t li = 0; li < nr_loops(); ++li) {
		os << loop_size(li) << " " << (loop_closed(li) ? 1 : 0) << " " << int(loop_color(li).R()) << " " << int(loop_color(li).G()) << " " << int(loop_color(li).B()) << std::endl;
		for (size_t vi = loop_begin(li); vi < loop_end(li); ++vi)
			os << vertex(vi) << std::endl;
	}
	return true;
}

/// return number of loops
size_t polygon::nr_loops() const 
{
	return loops.size(); 
}

/// return orientation
PolygonOrientation polygon::loop_orientation(size_t loop_idx) const 
{
	validate_loop_index(loop_idx); 
	return loops[loop_idx].orientation; 
}

/// return whether given loop is closed
bool polygon::loop_closed(size_t loop_idx) const 
{
	validate_loop_index(loop_idx); 
	return loops[loop_idx].is_closed; 
}

/// return loop color
const polygon::clr_type& polygon::loop_color(size_t loop_idx) const 
{ 
	validate_loop_index(loop_idx); 
	return loops[loop_idx].color; 
}

/// set a new loop color
void polygon::set_loop_color(size_t loop_idx, const clr_type& clr) 
{
	validate_loop_index(loop_idx);
	loops[loop_idx].color = clr;
	on_change_loop(loop_idx, PLA_COLOR);
}

/// return the number of vertices in given loop
size_t polygon::loop_size(size_t loop_idx) const 
{
	validate_loop_index(loop_idx); 
	return loops[loop_idx].nr_vertices; 
}

/// return index of first vertex of given loop
size_t polygon::loop_begin(size_t loop_idx) const 
{
	validate_loop_index(loop_idx); 
	return loops[loop_idx].first_vertex;	
}

/// return end of loop vertex index
size_t polygon::loop_end(size_t loop_idx) const 
{
	validate_loop_index(loop_idx); 
	return loops[loop_idx].first_vertex + loops[loop_idx].nr_vertices; 
}

/// try to close given loop and return whether this was successful
bool polygon::close_loop(size_t loop_idx) {
	validate_loop_index(loop_idx);
	if (loops[loop_idx].is_closed)
		return true;
	if (loops[loop_idx].nr_vertices < 3)
		return false;
	loops[loop_idx].is_closed = true;
	int flags = PLA_CLOSED;
	PolygonOrientation po_new = compute_orientation(loop_idx);
	if (po_new != loops[loop_idx].orientation) {
		loops[loop_idx].orientation = po_new;
		flags += PLA_ORIENTATION;
	}
	on_change_loop(loop_idx, flags);
	return true;
}

/// open given loop
void polygon::open_loop(size_t loop_idx)
{
	validate_loop_index(loop_idx);
	if (loops[loop_idx].is_closed) {
		loops[loop_idx].is_closed = false;
		int flags = PLA_CLOSED;
		if (loops[loop_idx].orientation != PO_UNDEF) {
			loops[loop_idx].orientation = PO_UNDEF;
			flags += PLA_ORIENTATION;
		}
		on_change_loop(loop_idx, flags);
	}
}

/// append a new loop composed of a single vertex, return index of new vertex
size_t polygon::append_loop(const vtx_type& vtx) 
{
	size_t vtx_idx = vertices.size();
	polygon_loop loop(vtx_idx, 1);
	loops.push_back(loop);
	vertices.push_back(vtx);
	after_insert_loop(nr_loops()-1);
	after_insert_vertex(vtx_idx);
	return vtx_idx;
}

/// remove a loop
void polygon::remove_loop(size_t loop_idx) 
{
	validate_loop_index(loop_idx);
	before_remove_loop(loop_idx);
	before_remove_vertex_range(loop_begin(loop_idx), loop_end(loop_idx));
	vertices.erase(vertices.begin() + loop_begin(loop_idx), vertices.begin() + loop_end(loop_idx));
	for (size_t li = loop_idx + 1; li < loops.size(); ++li)
		loops[li].first_vertex -= loop_size(loop_idx);
	loops.erase(loops.begin() + loop_idx);
}

/// return number of vertices
size_t polygon::nr_vertices() const 
{
	return vertices.size(); 
}

/// read only access to given vertex 
const polygon::vtx_type& polygon::vertex(size_t vtx_idx) const 
{
	validate_vertex_index(vtx_idx); 
	return vertices[vtx_idx]; 
}

/// set new vertex location
void polygon::set_vertex(size_t vtx_idx, const vtx_type& vtx, bool update_orientation)
{ 
	validate_vertex_index(vtx_idx);
	vertices[vtx_idx] = vtx;
	on_change_vertex(vtx_idx);
	if (update_orientation) {
		size_t loop_idx = find_loop(vtx_idx);
		PolygonOrientation new_po = compute_orientation(loop_idx);
		if (new_po != loops[loop_idx].orientation) {
			loops[loop_idx].orientation = new_po;
			on_change_loop(loop_idx, PLA_ORIENTATION);
		}
	}
}

/// return loop index of given vertex
size_t polygon::find_loop(size_t vtx_idx) const
{
	validate_vertex_index(vtx_idx);
	for (size_t li = 0; li < nr_loops(); ++li)
		if (vtx_idx >= loop_begin(li) && vtx_idx < loop_end(li))
			return li;
	// not found!!! should never happen
	assert(0);
	return size_t(-1);
}

/// append a new vertex to the given loop and return its index; if no loops exist, create new loop, if no loop is specified append vertex to last loop
size_t polygon::append_vertex_to_loop(const vtx_type& vtx, size_t loop_idx) 
{
	if (nr_loops() == 0)
		return append_loop(vtx);
	if (loop_idx == size_t(-1))
		loop_idx = nr_loops() - 1;
	validate_loop_index(loop_idx);
	size_t vtx_idx = loop_end(loop_idx);
	if (vtx_idx == vertices.size())
		vertices.push_back(vtx);
	else
		vertices.insert(vertices.begin()+vtx_idx, vtx);
	after_insert_vertex(vtx_idx);
	++loops[loop_idx].nr_vertices;
	on_change_loop(loop_idx, PLA_SIZE);
	for (size_t li = loop_idx + 1; li < nr_loops(); ++li) {
		++loops[li].first_vertex;
		on_change_loop(li, PLA_BEGIN);
	}
	return vtx_idx;
}

/// insert a new vertex before the given vertex
void polygon::insert_vertex(const vtx_type& vtx, size_t vtx_idx) 
{
	validate_vertex_index(vtx_idx);
	// add vertex
	if (vtx_idx == vertices.size())
		vertices.push_back(vtx);
	else
		vertices.insert(vertices.begin()+vtx_idx, vtx);
	after_insert_vertex(vtx_idx);
	// update loops
	for (size_t li = 0; li < nr_loops(); ++li)
		if (loop_begin(li) > vtx_idx) {
			++loops[li].first_vertex;
			on_change_loop(li, PLA_BEGIN);
		}
		else if (loop_end(li) > vtx_idx) {
			++loops[li].nr_vertices;
			on_change_loop(li, PLA_SIZE);
		}
}

/// remove a vertex, if loop size decreases to 2, loop is marked as open; if loop size decreases to 0, loop is removed
void polygon::remove_vertex(size_t vtx_idx) 
{
	validate_vertex_index(vtx_idx);
	// remove vertex
	before_remove_vertex(vtx_idx);
	vertices.erase(vertices.begin()+vtx_idx);
	// update loops
	for (size_t li = 0; li < nr_loops(); ++li)
		if (loop_begin(li) > vtx_idx) {
			--loops[li].first_vertex;
			on_change_loop(li, PLA_BEGIN);
		}
		else if (loop_end(li) > vtx_idx) {
			if (loop_size(li) == 1) {
				before_remove_loop(li);
				loops.erase(loops.begin() + li);
				--li;
			}
			else {
				int flags = PLA_SIZE;
				if (--loops[li].nr_vertices < 3) {
					loops[li].is_closed = false;
					flags += PLA_CLOSED;
				}
				on_change_loop(li, flags);
			}
		}
}
