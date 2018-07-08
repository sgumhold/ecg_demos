#pragma once

#include <vector>
#include <cgv/math/fvec.h>
#include <cgv/media/axis_aligned_box.h>
#include <cgv/media/color.h>
#include <cgv/signal/signal.h>

/// different polygon orientations (counter clock wise vs clock wise)
enum PolygonOrientation
{
	PO_UNDEF,
	PO_CCW,
	PO_CW
};

/// types used in polygon class
struct polygon_types
{
	typedef cgv::math::fvec<float, 2> vtx_type;
	typedef cgv::media::color<cgv::type::uint8_type> clr_type;
	typedef cgv::media::axis_aligned_box<float, 2> box_type;
};

/// simple struct to represent a loop in a polygon
struct polygon_loop : public polygon_types
{
	PolygonOrientation orientation;
	size_t first_vertex;
	size_t nr_vertices;
	clr_type color;
	bool is_closed;

	/// constuct loop 
	polygon_loop(size_t _fst_vtx, size_t _nr_vts, clr_type _clr = clr_type(0, 0, 0), bool _is_clsd = false);
};

/// flags for different attributes in a loop 
enum PolygonLoopAttributes
{
	PLA_ORIENTATION = 1,
	PLA_BEGIN = 2,
	PLA_SIZE = 4,
	PLA_COLOR = 8,
	PLA_CLOSED = 16
};


/// class to store and access a polygon that can contain several loops
class polygon : public polygon_types
{
protected:
	/// container to store loops
	std::vector<polygon_loop> loops;
	/// container to store vertices
	std::vector<vtx_type> vertices;
	/// assert that loop index is within valid range
	void validate_loop_index(size_t loop_idx) const;
	/// assert that vertex index is within valid range
	void validate_vertex_index(size_t vtx_idx) const;
	///  function to compute the orientation of a loop
	PolygonOrientation compute_orientation(size_t loop_idx) const;
public:
	/// construct empty polygon
	polygon();
	/// create polygon by subdivision of the unit circle with given number of vertices
	void generate_circle(size_t nr_vts);
	/// remove all loops and all vertices
	void clear();
	/// compute axis aligned bounding box of vertices
	box_type compute_box() const;
	/// center and scale polygon into box [-1,1]^2
	void center_and_scale_to_unit_box();
	/// read polygon from text file
	bool read(const std::string& file_name);
	/// write polygon to text file
	bool write(const std::string& file_name) const;
	/**@name signals used to inform about changes in the data structure*/
	//@{
	/// signal emitted after a new loop has been inserted, the argument is index of new loop
	cgv::signal::signal<size_t> after_insert_loop;
	/// signal emitted when a loop attribute changes, the first argument is index of changed loop and the second specifies the changed attributes via flags from PolygonLoopAttributes
	cgv::signal::signal<size_t, int> on_change_loop;
	/// signal emitted before a loop is removed, the argument is index of to be removed loop
	cgv::signal::signal<size_t> before_remove_loop;
	/// signal emitted after a new vertex has been inserted, the argument is index of new vertex
	cgv::signal::signal<size_t> after_insert_vertex;
	/// signal emitted when a vertex changed one of its coordinates, the argument is index of changed vertex
	cgv::signal::signal<size_t> on_change_vertex;
	/// signal emitted before a single vertex is removed, the argument is index of to be removed vertex
	cgv::signal::signal<size_t> before_remove_vertex;
	/// signal emitted before a range of vertices is removed, the arguments are begin and end index of to be removed vertex range
	cgv::signal::signal<size_t, size_t> before_remove_vertex_range;
	//@}
	/**@name access to loops*/
	//@{
	/// return number of loops
	size_t nr_loops() const;
	/// return orientation
	PolygonOrientation loop_orientation(size_t loop_idx) const;
	/// return whether given loop is closed
	bool loop_closed(size_t loop_idx) const;
	/// return loop color
	const clr_type& loop_color(size_t loop_idx) const;
	/// set a new loop color
	void set_loop_color(size_t loop_idx, const clr_type& clr);
	/// return the number of vertices in given loop
	size_t loop_size(size_t loop_idx) const;
	/// return index of first vertex of given loop
	size_t loop_begin(size_t loop_idx) const;
	/// return end of loop vertex index
	size_t loop_end(size_t loop_idx) const;
	/// try to close given loop and return whether this was successful
	bool close_loop(size_t loop_idx);
	/// open given loop
	void open_loop(size_t loop_idx);
	/// append a new loop composed of a single vertex, return index of new vertex
	size_t append_loop(const vtx_type& vtx);
	/// remove a loop
	void remove_loop(size_t loop_idx);
	//@}

	/**@name access to vertices*/
	//@{
	/// return number of vertices
	size_t nr_vertices() const;
	/// read only access to given vertex 
	const vtx_type& vertex(size_t vtx_idx) const;
	/// set new vertex location
	void set_vertex(size_t vtx_idx, const vtx_type& vtx, bool update_orientation = true);
	/// return loop index of given vertex
	size_t find_loop(size_t vtx_idx) const;
	/// append a new vertex to the given loop and return its index; if no loops exist, create new loop, if no loop is specified append vertex to last loop
	size_t append_vertex_to_loop(const vtx_type& vtx, size_t loop_idx = size_t(-1));
	/// insert a new vertex before the given vertex
	void insert_vertex(const vtx_type& vtx, size_t vtx_idx);
	/// remove a vertex, if loop size decreases to 2, loop is marked as open; if loop size decreases to 0, loop is removed
	void remove_vertex(size_t vtx_idx);
	//@}
};