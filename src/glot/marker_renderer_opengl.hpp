#pragma once

#include <glm/glm.hpp>
#include "shader_utils.hpp"
#include "timeseries.hpp"
#include "window.hpp"

enum class MarkerStyle
{
    Center,
    Left,
    Right
};

/**
 * @brief Routines and buffers for drawing makers - vertical lines with draggable handles and a little text label.
 */
class MarkerRendererOpenGL
{
  public:
    MarkerRendererOpenGL(Window &window);
    ~MarkerRendererOpenGL();
    void draw(const std::string &label,
              int position_px,
              int gutter_size_px,
              const glm::vec3 &colour,
              MarkerStyle style) const;

  private:
    unsigned int load_texture(const std::string &filename) const;

    Window &m_window;
    unsigned int m_handle_texture_center;
    unsigned int m_handle_texture_left;
    unsigned int m_handle_texture_right;
    unsigned int m_handle_vertex_buffer;
    unsigned int m_handle_vao;
    Program m_shader_program;
};
