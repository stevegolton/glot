#include <spdlog/spdlog.h>
#include <glm/gtx/matrix_transform_2d.hpp>
#include <imgui.h>
#include "bindings/imgui_impl_glfw.h"
#include "bindings/imgui_impl_opengl3.h"
#include "window.hpp"
#include "graph.hpp"

Window::Window(const Database &db, PluginManager &plugins)
    : _bgcolour(0.2f, 0.2f, 0.2f), _win_size(SCR_WIDTH, SCR_HEIGHT), _database(db),
      _plugin_manager(plugins), _plot_colour(1.0f, 0.5f, 0.2f), _minmax_colour(0.5f, 0.5f, 0.5f)
{
    m_window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "GLot", NULL, NULL);
    if (!m_window)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }

    // Register callbacks
    glfwSetWindowUserPointer(m_window, this);
    glfwMakeContextCurrent(m_window);
    glfwSetFramebufferSizeCallback(m_window, Window::framebuffer_size_callback);
    glfwSetCursorPosCallback(m_window, Window::cursor_pos_callback);
    glfwSetScrollCallback(m_window, Window::scroll_callback);
    glfwSetMouseButtonCallback(m_window, Window::mouse_button_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        glfwDestroyWindow(m_window);
        throw std::runtime_error("Failed to initialize GLAD");
    }

    // Our graph requires depth testing to be enabled in order to render correcly
    glEnable(GL_DEPTH_TEST);

    // Blending should be enabled for text to be rendered correclty
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Update settings
    _update_multisampling();
    _update_vsync();
    _update_bgcolour();

    m_graph = std::make_shared<GraphView>();
    m_graph->set_size(SCR_WIDTH, SCR_HEIGHT);

    update_viewport_matrix(SCR_WIDTH, SCR_HEIGHT);

    std::vector<glm::vec3> plot_colours = {
        glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)};

    const auto &data = _database.data();
    _ts.resize(data.size());
    auto col = plot_colours.begin();
    std::transform(data.begin(), data.end(), _ts.begin(), [&col, &plot_colours](const auto &ts) {
        TimeSeriesContainer cont;
        cont.name = ts.first;
        cont.ts = ts.second;
        cont.colour = *col++;
        cont.visible = true;
        cont.y_offset = 0.0f;
        if (col == plot_colours.end())
        {
            col = plot_colours.begin();
        }
        return cont;
    });
}

Window::~Window()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(m_window);
}

void Window::spin()
{
    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_graph->draw(_vp_matrix, _plot_width, _ts, _show_line_segments);
        render_imgui();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
    }
}

void Window::_update_multisampling()
{
    if (_enable_multisampling)
    {
        glEnable(GL_MULTISAMPLE);
    }
    else
    {
        glDisable(GL_MULTISAMPLE);
    }
}

void Window::_update_vsync() const
{
    glfwSwapInterval(_enable_vsync ? 1 : 0);
}

void Window::_update_bgcolour() const
{
    glClearColor(_bgcolour.r, _bgcolour.g, _bgcolour.b, 1.0f);
}

void Window::update_viewport_matrix(int width, int height)
{
    const glm::mat3 identity(1.0f);
    auto vp_matrix = glm::scale(identity, glm::vec2(width / 2, -height / 2));
    _vp_matrix = glm::translate(vp_matrix, glm::vec2(1, -1));
}

void Window::render_imgui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImVec2 menubar_size;
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Close"))
            {
                glfwSetWindowShouldClose(m_window, GL_TRUE);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Plugins"))
        {
            _plugin_manager.draw_menu();
            ImGui::EndMenu();
        }

        menubar_size = ImGui::GetWindowSize();
        ImGui::EndMainMenuBar();
    }

    ImGui::Begin("Info",
                 0,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoMove);
    ImGui::SetWindowPos(ImVec2(_win_size.x - ImGui::GetWindowWidth() - 10, menubar_size.y), true);

    if (ImGui::CollapsingHeader("Help", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BulletText("Left mouse + drag to pan");
        ImGui::BulletText("Scroll to zoom");
        ImGui::BulletText("Scroll on gutters to zoom individual axes");
    }

    if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("%.1f ms/frame (%.1f FPS)",
                    1000.0f / ImGui::GetIO().Framerate,
                    ImGui::GetIO().Framerate);

        auto viewmat = m_graph->view_matrix();
        ImGui::Text("View Matrix:");
        for (int i = 0; i < 3; i++)
        {
            ImGui::Text("%f %f %f", viewmat[0][i], viewmat[1][i], viewmat[2][i]);
        }

        auto cursor_gs = m_graph->cursor_graphspace(_vp_matrix);
        ImGui::Text("Cursor: %f %f", cursor_gs.x, cursor_gs.y);
    }

    if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Checkbox("Enable VSync", &_enable_vsync))
        {
            _update_vsync();
        }

        if (ImGui::Checkbox("Multisampling", &_enable_multisampling))
        {
            _update_multisampling();
        }

        if (ImGui::ColorEdit3("BG Colour", &(_bgcolour.x)))
        {
            _update_bgcolour();
        }

        ImGui::SliderInt("Line Width", &_plot_width, 1, 5);
        ImGui::Checkbox("Show Line Segments", &_show_line_segments);
        ImGui::ColorEdit3("MinMax Colour", &(_minmax_colour.x));
    }

    if (ImGui::CollapsingHeader("Plots", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (auto &plugin : _ts)
        {
            // Im ImGui, widgets need unique label names
            // Anything after the "##" is not displayed
            const auto label_name = "##" + plugin.name;
            ImGui::Checkbox(label_name.c_str(), &(plugin.visible));
            ImGui::SameLine();
            ImGui::ColorEdit3(
                plugin.name.c_str(), &(plugin.colour.x), ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine();
            ImGui::SliderFloat("Offset", &(plugin.y_offset), -10.0f, 10.0f);
        }
    }

    _plugin_manager.draw_dialogs();

    ImGui::Render();
}

void Window::framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    spdlog::info("Window resized: {}x{}px", width, height);
    glViewport(0, 0, width, height);
    Window *win = static_cast<Window *>(glfwGetWindowUserPointer(window));

    win->_win_size = glm::ivec2(width, height);

    // Update the graph to fill the screen
    win->m_graph->set_size(width, height);
    win->update_viewport_matrix(width, height);
}

void Window::cursor_pos_callback(GLFWwindow *window, double xpos, double ypos)
{
    Window *win = static_cast<Window *>(glfwGetWindowUserPointer(window));
    win->m_graph->cursor_move(win->_vp_matrix, xpos, ypos);
}

void Window::scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    ImGuiIO &io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    Window *win = static_cast<Window *>(glfwGetWindowUserPointer(window));
    win->m_graph->mouse_scroll(win->_vp_matrix, xoffset, yoffset);
}

void Window::mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    ImGuiIO &io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    Window *win = static_cast<Window *>(glfwGetWindowUserPointer(window));
    win->m_graph->mouse_button(button, action, mods);
}
