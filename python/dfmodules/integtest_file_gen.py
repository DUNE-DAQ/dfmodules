
def generate_hwmap_file(n_links, det_id = 3, links_per_ru_app=10):
    conf="# DRO_SourceID DetLink DetSlot DetCrate DetID DRO_Host DRO_Card DRO_SLR DRO_Link\n"
    for i in range(n_links):
        card = i // links_per_ru_app
        slr = (i % 10) // 5
        link = i % 5
        conf+=f"{i} {i % 2} {i // 2} 0 {det_id} localhost {card} {slr} {link}\n"
    return conf