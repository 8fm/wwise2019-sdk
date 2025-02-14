#pragma once

#include "Render.h"

#include "Utilities.h"

namespace d3d12
{
	class DynamicTexture2D : public IDynamicTexture2D
	{
	public:
		DynamicTexture2D(AkUInt32 width, AkUInt32 height);

		void UpdateResource(const void* buffer, size_t bufferPitch, size_t bufferLength);

		// When refcount is 0, the surface will be unavailable for further CPU usage,
		// but destruction of resource is deferred to end-of-frame
		void AddRef() override { ++m_uRefCnt; }
		void Release() override; // Schedules deferred destruction

		ID3D12DescriptorHeap* GetDescriptorHeap() { return m_SRVDescriptorHeap.Get(); }

	private:
		AkUInt32 m_width = 0;
		AkUInt32 m_height = 0;

		Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_textureUploadHeap;

		// Shader Resource Views descriptors
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_SRVDescriptorHeap{};

		AkUInt32 m_uRefCnt;
	};
}
