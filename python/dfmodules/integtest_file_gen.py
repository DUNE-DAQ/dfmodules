
def generate_hwmap_file(n_links, n_apps = 1, det_id = 3):
    conf="# DRO_SourceID DetLink DetSlot DetCrate DetID DRO_Host DRO_Card DRO_SLR DRO_Link\n"
    if n_links > 10 and n_apps == 1:
        print(f"n_links > 10! {n_links}. Adjusting to set n_apps={n_links//10} and n_links=10!")
        n_apps = n_links // 10
        n_links = 10

    sid = 0
    for app in range(n_apps):
        for link in range(n_links):
            card = app
            crate = app
            slr = link // 5
            link_num = link % 5
            conf+=f"{sid} {sid % 2} {sid // 2} {crate} {det_id} localhost {card} {slr} {link_num}\n"
            sid += 1
    return conf
