// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2015 Qianyi Zhou <Qianyi.Zhou@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include "Visualizer.h"

#include <IO/ClassIO/ImageIO.h>

namespace three{

bool Visualizer::InitOpenGL()
{
	if (glewInit() != GLEW_OK) {
		PrintError("Failed to initialize GLEW.\n");
		return false;
	}
		
	// depth test
	glEnable(GL_DEPTH_TEST);
	glClearDepth(1.0f);

	// pixel alignment
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// polygon rendering
	glEnable(GL_CULL_FACE);

	// glReadPixels always read front buffer
	glReadBuffer(GL_FRONT);

	return true;
}

void Visualizer::Render()
{
	glfwMakeContextCurrent(window_);
	
	view_control_ptr_->SetViewMatrices();

	Eigen::Vector3d background_color = RenderOption::DEFAULT_BACKGROUND_COLOR;
	glClearColor((GLclampf)background_color(0), (GLclampf)background_color(1),
			(GLclampf)background_color(2), 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for (const auto &renderer_ptr : renderer_ptrs_) {
		renderer_ptr->Render(*render_option_ptr_, *view_control_ptr_);
	}

	glfwSwapBuffers(window_);
}

void Visualizer::ResetViewPoint()
{
	view_control_ptr_->Reset();
	is_redraw_required_ = true;
}

void Visualizer::CaptureScreen(const std::string &filename/* = ""*/,
		bool do_render/* = true*/)
{
	std::string png_filename = filename;
	if (png_filename.empty()) {
		png_filename = "ScreenCapture_" + GetCurrentTimeStamp() + ".png";
	}
	Image screen_image;
	screen_image.PrepareImage(view_control_ptr_->GetWindowWidth(),
			view_control_ptr_->GetWindowHeight(), 3, 1);
	if (do_render) {
		Render();
		is_redraw_required_ = false;
	}
	glFinish();
	glReadPixels(0, 0, view_control_ptr_->GetWindowWidth(), 
			view_control_ptr_->GetWindowHeight(), GL_RGB, GL_UNSIGNED_BYTE,
			screen_image.data_.data());

	// glReadPixels get the screen in a vertically flipped manner
	// Thus we should flip it back.
	Image png_image;
	png_image.PrepareImage(view_control_ptr_->GetWindowWidth(),
			view_control_ptr_->GetWindowHeight(), 3, 1);
	int bytes_per_line = screen_image.BytesPerLine();
	for (int i = 0; i < screen_image.height_; i++) {
		memcpy(png_image.data_.data() + bytes_per_line * i,
				screen_image.data_.data() + bytes_per_line * 
				(screen_image.height_ - i - 1), bytes_per_line);
	}

	PrintDebug("[Visualizer] Screen capture to %s\n", png_filename.c_str());
	WriteImage(png_filename, png_image);
}

void Visualizer::CaptureDepth(const std::string &filename/* = ""*/,
		bool do_render/* = true*/, double depth_scale/* = 1000.0*/)
{
	std::string png_filename = filename;
	if (png_filename.empty()) {
		png_filename = "DepthCapture_" + GetCurrentTimeStamp() + ".png";
	}
	Image depth_image;
	depth_image.PrepareImage(view_control_ptr_->GetWindowWidth(),
			view_control_ptr_->GetWindowHeight(), 1, 4);
	if (do_render) {
		Render();
		is_redraw_required_ = false;
	}
	glFinish();
	glReadPixels(0, 0, view_control_ptr_->GetWindowWidth(), 
			view_control_ptr_->GetWindowHeight(), GL_DEPTH_COMPONENT, GL_FLOAT,
			depth_image.data_.data());

	// glReadPixels get the screen in a vertically flipped manner
	// We should flip it back, and convert it to the depth value
	Image png_image;
	double z_near = view_control_ptr_->GetZNear();
	double z_far = view_control_ptr_->GetZFar();

	png_image.PrepareImage(view_control_ptr_->GetWindowWidth(),
			view_control_ptr_->GetWindowHeight(), 1, 2);
	for (int i = 0; i < depth_image.height_; i++) {
		float *p_depth = (float *)(depth_image.data_.data() + 
				depth_image.BytesPerLine() * (depth_image.height_ - i - 1));
		uint16_t *p_png = (uint16_t *)(png_image.data_.data() +
				png_image.BytesPerLine() * i);
		for (int j = 0; j < depth_image.width_; j++) {
			if (p_depth[j] == 1.0) {
				continue;
			}
			double z_depth = 2.0 * z_near * z_far / 
					(z_far + z_near - (2.0 * (double)p_depth[j] - 1.0) * 
					(z_far - z_near));
			p_png[j] = std::min((uint16_t)std::round(depth_scale * z_depth),
					(uint16_t)INT16_MAX);
		}
	}

	PrintDebug("[Visualizer] Depth capture to %s\n", png_filename.c_str());
	WriteImage(png_filename, png_image);
}

}	// namespace three