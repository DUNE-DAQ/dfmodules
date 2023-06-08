import daqconf.detreadoutmap as dromap
import json

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

def generate_dromap_contents(n_streams, n_apps = 1, det_id = 3, app_type = "eth", app_host = "localhost",
                             eth_protocol = "udp", flx_mode = "fix_rate", flx_protocol = "full"):
    the_map = dromap.DetReadoutMapService()
    source_id = 0
    for app in range(n_apps):
        for stream in range(n_streams):
            geo_id = dromap.GeoID(det_id, app, 0, stream)
            if app_type == 'flx':
                # untested!
                the_map.add_srcid(source_id, geo_id, app_type, host=app_host,
                                  protocol=flx_protocol, mode=flx_mode,
                                  card=app, slr=(stream // 5), link=(stream % 5))
            else:
                the_map.add_srcid(source_id, geo_id, app_type, protocol=eth_protocol, rx_host=app_host,
                                  rx_iface=app, rx_mac=f"00:00:00:00:00:0{app}", rx_ip=f"0.0.0.{app}")
            source_id += 1
    return json.dumps(the_map.as_json(), indent=4)
