#include "vk_buffer.hpp"
#include "vk_context.hpp"

void vka::AllocatedBuffer::Resize(uint64_t NewSize)
{
    TotalSize = NewSize;

    if(Buffer != nullptr)
    {
        DeferredDestructionQueue.enqueue([Allocator = Allocator, Buffer = Buffer, Allocation = Allocation](){
            Allocator.destroyBuffer(Buffer, Allocation);
        });
    }

    if(TotalSize == 0) [[unlikely]]
    {
        Buffer = nullptr;
        Allocation = nullptr;
        TotalSize = 0;
        UsedSize = 0;
    }
    else
    {
        vk::CommandBuffer CommandBuffer = vkContext->RequestTransferCommandBuffer("buffer data copy");
        //vkContext->SubmitTransferCommands(CommandBuffer);

        auto BufferCreateInfo = vk::BufferCreateInfo{}
                .setSize(TotalSize)
                .setUsage(BufferUsage);

        auto AllocationCreateInfo = vma::AllocationCreateInfo{}
                .setFlags(AllocationFlags)
                .setUsage(MemoryUsage);

        vkResultCheck = Allocator.createBuffer(&BufferCreateInfo, &AllocationCreateInfo, &Buffer, &Allocation, nullptr);
    }
}
