#pragma once

#include "Utilities.h"

#include <vector>

namespace d3d12
{
	// Graphics pipeline to draw lines using Shader_Line2DSimple*.hlsl
	class LineGraphicsPipeline
	{
	public:
		void Init();

		void Term();

		void SendDrawCommands();

		void AddLine(float x1, float y1, float x2, float y2, float color[3])
		{
			m_vertices.push_back({ { x1, y1 }, { color[0], color[1], color[2] } });
			m_vertices.push_back({ { x2, y2 }, { color[0], color[1], color[2] } });
		}

		void ClearLines() { m_vertices.clear(); }

	private:
		// Root signature
		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

		// Pipeline state object.
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;

		// Vertex buffer (one per in-flight frame)
		Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffers[RenderingSettings::kConcurrentFrameCount];
		Microsoft::WRL::ComPtr<ID3D12Resource> m_intermediateVertexBuffers[RenderingSettings::kConcurrentFrameCount];
		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferViews[RenderingSettings::kConcurrentFrameCount];

		struct LineVertex
		{
			vec2 pos;
			vec3 color;
		};
		std::vector<LineVertex> m_vertices;
	};
}
