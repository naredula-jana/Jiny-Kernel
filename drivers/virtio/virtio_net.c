
/*
 *  Select the queue we're interested in
 370        iowrite16(index, vp_dev->ioaddr + VIRTIO_PCI_QUEUE_SEL);
 371
 372         Check if queue is either not available or already active.
 373        num = ioread16(vp_dev->ioaddr + VIRTIO_PCI_QUEUE_NUM);
 374        if (!num || ioread32(vp_dev->ioaddr + VIRTIO_PCI_QUEUE_PFN))
 375                return ERR_PTR(-ENOENT);
 376
 377         allocate and fill out our structure the represents an active
 378         * queue
 379        info = kmalloc(sizeof(struct virtio_pci_vq_info), GFP_KERNEL);
 380        if (!info)
 381                return ERR_PTR(-ENOMEM);
 382
 383        info->queue_index = index;
 384        info->num = num;
 385        info->msix_vector = msix_vec;
 386
 387        size = PAGE_ALIGN(vring_size(num, VIRTIO_PCI_VRING_ALIGN));
 388        info->queue = alloc_pages_exact(size, GFP_KERNEL|__GFP_ZERO);
 389        if (info->queue == NULL) {
 390                err = -ENOMEM;
 391                goto out_info;
 392        }
 393
 394         activate the queue
 395        iowrite32(virt_to_phys(info->queue) >> VIRTIO_PCI_QUEUE_ADDR_SHIFT,
 396                  vp_dev->ioaddr + VIRTIO_PCI_QUEUE_PFN);
 397
 398         create the vring
 399        vq = vring_new_virtqueue(info->num, VIRTIO_PCI_VRING_ALIGN,
 400                                 vdev, info->queue, vp_notify, callback, name);

 */






