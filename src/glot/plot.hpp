#pragma once

#include <glm/glm.hpp>
#include "shader_utils.hpp"
#include <database/timeseries.hpp>
#include "window.hpp"
#include "view.hpp"
#include <boost/signals2.hpp>

class Plot : public View
{
  public:
    Plot();
    ~Plot();
    Plot(const Plot &) = delete;
    Plot &operator=(const Plot &) = delete;
    Plot(Plot &&);
    Plot &operator=(Plot &&) = delete;

    glm::dvec2 position() const override;
    void set_position(const glm::dvec2 &position) override;

    glm::dvec2 size() const override;
    void set_size(const glm::dvec2 &size) override;

    void draw(const Window &window) const override;

    void draw(const glm::mat3 &view_matrix,
              const std::vector<TSSample> &data,
              int plot_width,
              glm::vec3 plot_colour,
              float y_offset,
              bool show_line_segments) const;

    void on_scroll(const Window &, double, double) override;

    boost::signals2::signal<void(double)> on_zoom;

  private:
    static constexpr size_t COLS_MAX = 8192; // Number of preallocated buffer space for samples
    unsigned int m_vao;
    unsigned int m_vbo;
    Program m_shader;
    glm::dvec2 m_position;
    glm::dvec2 m_size;
};
