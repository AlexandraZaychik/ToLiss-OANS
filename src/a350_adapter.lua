-- Adapter between A350's own map pipeline and the existing A320 ND host.
-- Coordinates enter in metres (east/north), output uses the native 600 square.
-- A350 algorithms retain their own 300-wide, y-down drawing coordinates.

local function run_job(fn)
    local co = coroutine.create(fn)
    repeat
        if native_cancelled() then error('airport load cancelled') end
        local ok, err = coroutine.resume(co)
        if not ok then error(err) end
    until coroutine.status(co) == 'dead'
end

-- Label names are immutable for a loaded airport. Compute UTF-8 length and
-- concatenated variants once; rendering only switches existing string values.
local function prepare_stand_names(G)
    for _, label in ipairs(G.stand_lbls) do
        local names = {}
        for _, name in ipairs(label.names) do
            local _, characters = name:gsub('[^\128-\191]', '')
            if characters > 8 then
                name = name:match('([^ ]+) *$') or name
                _, characters = name:gsub('[^\128-\191]', '')
                if characters > 8 then name = name:match('[0-9].*') or name end
            end
            names[#names+1] = name
        end
        label.full_text, label.simple_text = label.text, table.concat(names, '_')
    end
    S.simple_stand_names = false -- build_geo leaves the full names selected.
end

local function select_stand_names(G, simple)
    if S.simple_stand_names == simple then return end
    for _, label in ipairs(G.stand_lbls) do
        label.text = simple and label.simple_text or label.full_text
    end
    S.simple_stand_names = simple
end

-- Dynamic pointer is separate from the cached airport buffer.
function a350_cursor(x, y, horizontal, yellow)
    GLB.n, GLB.cap = 0, 180000
    native_draw_reset()
    GLB.wu, GLB.wv, GLB.px, GLB.ts = TF.wu, TF.wv, 1, 2
    DL.begin({2,0,0,0,-2,600,0,0}, 300, 300)
    x, y = x/2, (600-y)/2
    local col = yellow and 0xFF00FFFF or 0xFFFF00FF -- packed ABGR
    local function line(dx1, dy1, dx2, dy2)
        if horizontal then dx1, dy1, dx2, dy2 = dy1, -dx1, dy2, -dx2 end
        DL.line(x+dx1,y+dy1,x+dx2,y+dy2,col,.6)
    end
    -- Rotate the inward chevrons about the unchanged click hotspot.
    for _, d in ipairs({-1,1}) do
        line(-7.5,d*10,0,d*3)
        line(0,d*3,7.5,d*10)
    end
    DL.line(x-.2,y,x+.2,y,col,.8)
    return GLB.n
end

function a350_plan_notice(mode, outside)
    GLB.n, GLB.cap = 0, 180000
    native_draw_reset()
    GLB.wu, GLB.wv, GLB.px, GLB.ts = TF.wu, TF.wv, 1, 2
    DL.begin({2,0,0,0,-2,600,0,0}, 300, 300)
    if mode == 'plan' and outside then
        local cap = 8.8
        local w = text_w('ACTIVE F-PLN',cap)
        -- User's flight-deck reference: a left-aligned two-line block at the
        -- top centre, between speed information and the airport identifier.
        local x = (300-w)/2
        DL.rectf(x-4,10,x+w+4,34,MC.bg)
        DL.text(x,12,MC.white,'ARPT NOT IN',cap)
        DL.text(x,22,MC.white,'ACTIVE F-PLN',cap)
    end
    return GLB.n
end

function a350_load_font(fontdir)
    SCRIPTS = fontdir .. '/'
    pnl_font_load()
    assert(TF.v == 2 and TF.data, 'A350 B612 atlas required')
    TF.ok = true
    TF.data = nil -- native atlas uploader owns the same unmodified pixel data
end

function a350_load(file, offset, icao, fontdir, buildingsfile)
    a350_load_font(fontdir)
    run_job(function()
        local apt = assert(parse_airport_block{file=file, off=offset, icao=icao})
        -- Size codes come from row 1301; unknown sizes cannot establish >= C.
        local stands = {}
        for _, st in ipairs(apt.stands) do
            if st.cat and st.cat >= 'C' and st.cat <= 'F' then stands[#stands+1] = st end
        end
        apt.stands = stands
        apt.nets = {}
        if apt.n_nodes > 0 then
            apt.nets[1] = {nodes=apt.nodes, edges=apt.edges, n_nodes=apt.n_nodes, src=file}
        end
        if #apt.roads > 0 then apt.road_net = {nodes=apt.nodes, roads=apt.roads} end
        prepare_runway_ends(apt)
        build_geo(apt)
        S.apt = apt
        if buildingsfile and buildingsfile ~= '' then
            local f = io.open(buildingsfile, 'rb')
            if f then
                local txt = f:read('*a'); f:close()
                local buildings, err = A350_buildings_build(apt, txt)
                assert(buildings, err)
                apt.geo.buildings = buildings
            end
        end
    end)
    local G = S.apt.geo
    prepare_stand_names(G)
    return G.ref.lat, G.ref.lon, #G.polys, #G.stand_areas, #G.stand_lbls,
           G.buildings and #G.buildings.terms or 0, G.buildings and #G.buildings.bldg or 0,
           S.apt.tower and S.apt.tower.lat, S.apt.tower and S.apt.tower.lon
end

-- The original checkbox also works before an airport is available.
function a350_oans_switch(on, partial)
    GLB.n, GLB.cap = 0, 180000
    native_draw_reset()
    GLB.wu, GLB.wv = TF.wu, TF.wv
    GLB.px, GLB.ts = 1, 2
    DL.begin({2,0,0,0,-2,600,0,0}, 300, 300)
    FONT_H = TXT.h
    nd_layout()
    -- Compact A320 switch: final baseline is 4 ND pixels above the test layout.
    local right = 299
    local box = {right-(2+7+2+text_w("OANS", TXT.small)+1),
                 OANS_R[2]+5, right, OANS_R[4]+5}
    nd_key(box, false)
    local bs = 7
    local bx, by = box[1]+2, (box[2]+box[4])/2-bs/2+0.5
    DL.rect(bx, by, bx+bs, by+bs, MC.sel, 0, 0, 0.9)
    if on and partial then
        DL.circle(bx+bs/2, by+bs/2, 1.5, MC.green, 16, 2.0)
    elseif on then
        DL.line(bx+1.4, by+3.8, bx+3.0, by+5.6, MC.green, 1.4)
        DL.line(bx+3.0, by+5.6, bx+5.9, by+1.4, MC.green, 1.4)
    end
    DL.text(bx+bs+2, (box[2]+box[4])/2-FONT_H/2+0.5, MC.sel, "OANS", TXT.small)
    return GLB.n,
           box[1]*2, 600-box[4]*2, box[3]*2, 600-box[2]*2
end

-- Font loader's upstream filename is kept; only this path is mapped by host.
-- Clip the runway centreline to the map actually visible below the header and
-- above the controls/panel. The compass circle is drawn over this rectangular
-- map; it is not a clipping boundary.
local function clip_segment(x,y,dx,dy,l,t,r,b)
    local lo,hi=0,1
    local function axis(p,d,a,z)
        if math.abs(d)<1e-9 then return p>=a and p<=z end
        local u,v=(a-p)/d,(z-p)/d
        if u>v then u,v=v,u end
        lo,hi=math.max(lo,u),math.min(hi,v)
        return lo<=hi
    end
    if l>r or t>b or not axis(x,dx,l,r) or not axis(y,dy,t,b) then return nil end
    return lo,hi
end

function MAPX.visible_runway_label(C,r,tw,cap)
    local x,y=C.XY(r.x1,r.y1)
    local ex,ey=C.XY(r.x2,r.y2)
    local dx,dy=ex-x,ey-y
    local len=math.sqrt(dx*dx+dy*dy)
    if len<1e-6 then return nil end
    local l,t,rr,b=C.x0,C.y0+25,C.x0+C.cw,C.map_bottom
    local lo,hi=clip_segment(x,y,dx,dy,l,t,rr,b)
    if not lo or (hi-lo)*len<tw+4 then return nil end
    -- Usually the visible midpoint fits as-is. Near a map edge, keep the
    -- complete rotated label inside the map by sliding along the runway.
    local ux,uy=dx/len,dy/len
    local w,h=tw/2+2,cap/2+2
    local hx,hy=math.abs(ux)*w+math.abs(uy)*h,math.abs(uy)*w+math.abs(ux)*h
    local a,z=clip_segment(x,y,dx,dy,l+hx,t+hy,rr-hx,b-hy)
    if not a then return nil end
    local mid=math.max(a,math.min(z,(lo+hi)/2))
    local p={x+dx*mid,y+dy*mid,(hi-lo)*len,x+dx*lo,y+dy*lo,x+dx*hi,y+dy*hi}
    C.pair_segments[r]=p
    return p
end

function MAPX.selected_runway_id()
    local fe=FMS.rwy_end(S.apt)
    return S.map_runway or (fe and fe.id)
end
function MAPX.runway_ident_colour(id)
    return id==MAPX.selected_runway_id() and MC.green or MC.white
end
function MAPX.draw_runway_pair(C,x,y,angle,r)
    local p=C.pair_segments[r]
    local first,second=r.id1,r.id2
    local sx,sy,ex,ey=p[4],p[5],p[6],p[7]
    -- The upstream label direction is flipped to keep the text upright.
    -- Flip the endpoint names as well, so each name stays at its physical end.
    if (ex-sx)*math.cos(angle)+(ey-sy)*math.sin(angle)<0 then
        first,second=second,first
        sx,sy,ex,ey=ex,ey,sx,sy
    end
    local pieces={first,'-',second}
    local total=DL.text_rot_w(table.concat(pieces),TXT.small)
    local offset=-total/2
    for _,s in ipairs(pieces) do
        local w=DL.text_rot_w(s,TXT.small)
        local d=offset+w/2
        DL.text_rot(x+math.cos(angle)*d,y+math.sin(angle)*d,angle,MAPX.runway_ident_colour(s),s,TXT.small)
        offset=offset+w
    end
    local selected=MAPX.selected_runway_id()
    if selected~=r.id1 and selected~=r.id2 then selected='' end
    C.label_info[#C.label_info+1]={first,second,selected or '',x,y,sx,sy,ex,ey}
end

-- User video reference: green circled five-point star with an upright TWR label.
-- Position comes from an unnamed or tower-named apt.dat row 14 viewpoint.
-- This simulator viewpoint is a fallback, not surveyed physical-tower geometry.
function MAPX.draw_tower(C, boeing)
    local tower = S.apt.tower
    if not tower then return end
    if boeing and C.rng>2 then return end
    local x,y = C.XY(C.G.W(tower.lat,tower.lon))
    local cap = TXT.small
    local width = boeing and 0 or text_w('TWR',cap)
    local bounds = {x-math.max(6,width/2+2), y-7, x+math.max(6,width/2+2), y+(boeing and 7 or 8+cap)}
    -- Keep the full symbol and label inside the map, clear of host controls.
    if bounds[1] < C.x0 or bounds[3] > C.x0+C.cw or bounds[2] < C.y0+25 or bounds[4] > C.map_bottom then return end
    for _, r in ipairs(C.placed) do
        if bounds[1] < r[3] and bounds[3] > r[1] and bounds[2] < r[4] and bounds[4] > r[2] then return end
    end
    C.placed[#C.placed+1] = bounds
    DL.circle(x,y,4.8,MC.bg,24,2.8)
    local colour=boeing and MC.white or MC.green
    DL.circle(x,y,4.8,colour,24,1.0)
    local star = {}
    for i = 0,9 do
        local angle = -math.pi/2+i*math.pi/5
        local radius = i%2 == 0 and 3.8 or 1.5
        star[#star+1] = {x+math.cos(angle)*radius,y+math.sin(angle)*radius}
    end
    for i,p in ipairs(star) do
        local q = star[i%#star+1]
        DL.tri(x,y,p[1],p[2],q[1],q[2],colour)
    end
    local ty = y+7-(FONT_H-cap)/2
    if not boeing then DL.text(x-width/2,ty,MC.green,'TWR',cap) end
end

-- Bundled source: ChartNavigation/resources/documents/wtc.json (fast-is-slow).
-- Temporarily hide traffic metadata labels; retain their data and drawing code.
local traffic_labels_enabled = false
local traffic_wtc = {}
function a350_load_wtc(path)
    traffic_wtc = {}
    local file = io.open(path, 'rb')
    if not file then return end
    local bytes = file:read('*a'); file:close()
    -- This asset is a flat ICAO-designator -> category string map. Accept only
    -- bounded designators and known categories; never execute asset contents.
    for designator, category in bytes:gmatch('"([A-Z0-9]+)"%s*:%s*"([LMHJ])"') do
        if #designator<=7 then traffic_wtc[designator]=category end
    end
end

local function traffic_label(C, target, x, y)
    local cap = TXT.small
    local first = target[4] ~= '' and target[4] or '---'
    local wake = traffic_wtc[target[6]] or '?'
    local second = (target[5] >= 0 and tostring(target[5]) or '---') .. ' ' .. wake
    local width = math.max(text_w(first,cap),text_w(second,cap))
    local line = cap+2
    local height = line+cap
    -- Upright two-line labels: try either side, then above/below the symbol.
    local candidates = {{x+10,y-height/2},{x-10-width,y-height/2},
        {x-width/2,y-10-height},{x-width/2,y+10},
        {x+10,y-10-height},{x-10-width,y-10-height},
        {x+10,y+10},{x-10-width,y+10}}
    for _, p in ipairs(candidates) do
        local box = {p[1]-1,p[2]-1,p[1]+width+1,p[2]+height+1}
        local clear = box[1]>=C.x0+2 and box[3]<=C.x0+C.cw-2 and box[2]>=26 and box[4]<=C.map_bottom-2
        if clear and C.Rr>0 then
            for _, corner in ipairs({{box[1],box[2]},{box[3],box[2]},{box[1],box[4]},{box[3],box[4]}}) do
                if (corner[1]-C.ax)^2+(corner[2]-C.ay)^2>(C.Rr-2)^2 then clear=false end
            end
        end
        if clear then
            for _, r in ipairs(C.placed) do
                if box[1]<r[3] and box[3]>r[1] and box[2]<r[4] and box[4]>r[2] then clear=false;break end
            end
        end
        if clear then
            C.placed[#C.placed+1]=box
            DL.text(p[1],p[2],MC.white,first,cap)
            DL.text(p[1],p[2]+line,MC.white,second,cap)
            return
        end
    end
end

-- 787 full-screen MAP compass: ticks and headings lie inside the arc.
local function boeing_arc_compass(C, mag)
    local ax,ay,R=C.ax,C.ay,C.Rr
    -- Full MFD reaches the side edges; the split pane uses a shallow arc.
    local limit=C.cw>300 and 90 or 62
    local px,py
    for a=-limit,limit,2 do
        local t=a*D2R
        local x,y=ax+R*math.sin(t),ay-R*math.cos(t)
        if px then DL.line(px,py,x,y,MC.ring,1.0) end
        px,py=x,y
    end
    for deg=math.ceil((mag-limit)/5)*5,mag+limit,5 do
        local t=(deg-mag)*D2R
        local sx,sy=math.sin(t),-math.cos(t)
        local d=deg%360
        local major=d%10==0
        local length=major and 8 or 4
        DL.line(ax+R*sx,ay+R*sy,ax+(R-length)*sx,ay+(R-length)*sy,MC.ring,1)
        if d%30==0 then
            DL.text_rot(ax+(R-18)*sx,ay+(R-18)*sy,t,MC.white,tostring(math.floor(d/10)),8.5)
        end
    end
end

-- The 787 uses individual runway-end identifiers and unboxed cyan taxiway
-- identifiers. Do not reuse the A350 runway-pair or yellow taxiway labels.
local function boeing_labels(C)
    if C.rng>2 then return end
    local cyan=0xFFFFFF00 -- ABGR
    local function available(x,y,hw,hh)
        return x-hw>=C.x0 and x+hw<=C.x0+C.cw and y-hh>=25 and y+hh<=C.map_bottom
    end
    for _,r in ipairs(C.vis or {}) do
        for endi=1,2 do
            local id=endi==1 and r.id1 or r.id2
            -- Use the same stopway/blast-pad extent as MAPX.runways, rather
            -- than placing the placard at the end of the runway pavement.
            local extension=endi==1 and (r.blast1 or 0) or (r.blast2 or 0)
            local direction=endi==1 and -1 or 1
            local wx=(endi==1 and r.x1 or r.x2)+direction*r.ux*extension
            local wy=(endi==1 and r.y1 or r.y2)+direction*r.uy*extension
            local x,y=C.XY(wx,wy)
            -- The placard's long axis is perpendicular to this runway end.
            local ix,iy=C.XY(endi==1 and r.x2 or r.x1,endi==1 and r.y2 or r.y1)
            local angle=math.atan2(iy-y,ix-x)+math.pi/2
            angle=(angle+math.pi/2)%math.pi-math.pi/2 -- keep text readable
            local ux,uy=math.cos(angle),math.sin(angle)
            local vx,vy=-uy,ux
            local cap=6
            local w=DL.text_rot_w(id,cap)/2+3
            local h=cap/2+2
            local hw,hh=math.abs(ux)*w+math.abs(vx)*h,math.abs(uy)*w+math.abs(vy)*h
            if available(x,y,hw,hh) then
                local points={}
                for side=-1,1,2 do
                    for i=0,8 do
                        local a=(side==1 and -math.pi/2 or math.pi/2)+i*math.pi/8
                        local along=side*(w-h)+h*math.cos(a)
                        local across=h*math.sin(a)
                        points[#points+1]={x+ux*along+vx*across,y+uy*along+vy*across}
                    end
                end
                MAPX.poly_fill(points,MC.bg)
                MAPX.poly_line(points,cyan,0.8)
                DL.text_rot(x,y,angle,MC.white,id,cap)
                C.placed[#C.placed+1]={x-hw,y-hh,x+hw,y+hh}
            end
        end
    end
    if C.rng <= 0.5 then
        for _, l in ipairs(C.G.stand_lbls) do
            if l.x>C.vminx and l.x<C.vmaxx and l.y>C.vminy and l.y<C.vmaxy then
                local x,y=C.XY(l.x,l.y)
                local w=text_w(l.text,5)/2
                local box={x-w-2,y-5,x+w+2,y+5}
                if available(x,y,w+2,5) and not overlap(box,C.placed) then
                    DL.text(x-w,y-FONT_H/2,cyan,l.text,5)
                    C.placed[#C.placed+1]=box
                end
            end
        end
    end
    local per,count={},0
    for _,e in ipairs(C.G.lbl) do
        if count>=110 or e.len<18/C.sc then break end
        local wx,wy=(e.x1+e.x2)/2,(e.y1+e.y2)/2
        if wx>C.vminx and wx<C.vmaxx and wy>C.vminy and wy<C.vmaxy then
            local x,y=C.XY(wx,wy)
            local last=per[e.name]
            local w=text_w(e.name,5)/2
            local box={x-w-2,y-5,x+w+2,y+5}
            if available(x,y,w+2,5) and not overlap(box,C.placed) and
               (not last or (x-last[1])^2+(y-last[2])^2>85^2) then
                DL.text(x-w,y-FONT_H/2,cyan,e.name,5)
                C.placed[#C.placed+1]=box
                per[e.name]={x,y}
                count=count+1
            end
        end
    end
end

-- The 787 ownship is an open white triangle, with its tip along true heading
-- relative to the map. Traffic keeps its existing separate symbol.
local function boeing_ownship(x,y,angle)
    local fx,fy=math.sin(angle),-math.cos(angle)
    local rx,ry=math.cos(angle),math.sin(angle)
    local tx,ty=x+fx*12,y+fy*12
    local lx,ly=x-fx*10-rx*8,y-fy*10-ry*8
    local qx,qy=x-fx*10+rx*8,y-fy*10+ry*8
    DL.line(tx,ty,lx,ly,MC.bg,2.5);DL.line(lx,ly,qx,qy,MC.bg,2.5);DL.line(qx,qy,tx,ty,MC.bg,2.5)
    DL.line(tx,ty,lx,ly,MC.white,0.9);DL.line(lx,ly,qx,qy,MC.white,0.9);DL.line(qx,qy,tx,ty,MC.white,0.9)
end

function a350_render(cx, cy, scale, angle, ax, ay, lat, lon, psi, mag, mode, rng, crosses, flags, radius, bottom, fms_airport, fms_runway, alerts, blink, outside_plan, map_runway, traffic, position_valid, simple_stand_names, layer, detail_quality, width, boeing_full_compass)
    GLB.n, GLB.cap = 0, detail_quality == 0 and math.huge or (detail_quality == 1 and 270000 or 180000)
    native_draw_reset()
    GLB.wu, GLB.wv = TF.wu, TF.wv
    GLB.px, GLB.ts = 1, 2
    -- A350 300-unit width -> A320 600-unit width. Square target changes only
    -- the host's viewport height and anchor, never map shapes or label rules.
    DL.begin({2,0,0,0,-2,600,0,0}, 300, 300)
    FONT_H = TXT.h
    DR.lat[0], DR.lon[0], DR.psi[0], DR.mag[0] = lat, lon, psi, mag
    FMS.airport, FMS.runway = fms_airport, fms_runway
    S.map_runway=map_runway~='' and map_runway or nil
    local G = S.apt.geo
    -- The palette and temporary layer substitutions are local to this render;
    -- the shared A350 map must retain its own colours and source geometry.
    local saved_mc,saved_geo={},{}
    if boeing_full_compass then
        local palette={apron=0xFF646464,shoulder=0xFF646464,stand_area=0xFF646464,
            stand_line=0xFF646464,runway=0xFFC0C0C0,blast=0xFF999999,
            chevron=rng<=2 and 0xFF555555 or 0xFF999999,terminal=0xFFFFFF00,
            acft=0xFFFFFFFF}
        for k,v in pairs(palette) do saved_mc[k]=MC[k];MC[k]=v end
        for _,key in ipairs({'stand_areas','road','wline','dash_y','tw_net','tl_m','tl'}) do
            saved_geo[key]=G[key];G[key]={}
        end
        saved_geo.buildings=G.buildings
        if G.buildings then G.buildings={terms=rng<=2 and G.buildings.terms or {},bldg={}} end
    end
    S.marks[S.apt.icao] = {crosses=crosses, flags=flags}
    local x0, x1 = (600-(width or 600))/4, (600+(width or 600))/4
    -- The upstream primitives clip to a 300-unit square. A wide 787 MFD
    -- extends beyond that square on both sides; retain geometry there too.
    DL.pad = math.max(0,-x0)
    local C = {G=G, x0=x0, y0=0, cw=x1-x0, ch=300, nd={side=0}, placed={}, hit={},
               wcx=cx, wcy=cy, sc=scale/2, hv=angle/D2R, hc=math.cos(angle), hs=math.sin(angle),
               ax=ax/2, ay=(600-ay)/2, mode=mode, rng=rng, Rr=radius/2,
               map_bottom=(600-bottom)/2,pair_segments={},label_info={},
               detail_quality=detail_quality, boeing=boeing_full_compass}
    MAPX.setup(C)
    DL.clip = {x0*2,x1*2,0,600}
    if layer == 1 then
        -- Match map_texture_margin=64 host pixels. Keep label placement in the
        -- actual ND viewport, but retain surfaces/text beyond its edges for
        -- per-frame texture reprojection during turns.
        DL.clip = {x0*2-64,x1*2+64,-64,664}
        DL.pad = math.max(0,-x0)+32 -- local units; retain the texture margin.
    end
    local surface_vertices = 0
    if layer == 0 or layer == 1 then MAPX.surfaces(C) end
    if layer == 1 then surface_vertices = GLB.n end
    if layer == 0 or layer == 5 then
    -- Keep the original runway geometry, projection, polygon fill and clipping.
    -- The host supplies the ANF threats instead of the upstream first-hit RWYA.
    if blink then
        for _, hit in ipairs(alerts or {}) do
            for _, r in ipairs(G.rwys) do
                if (r.id1 == hit[1] and r.id2 == hit[2]) or (r.id2 == hit[1] and r.id1 == hit[2]) then
                    local dx, dy = r.x2-r.x1, r.y2-r.y1
                    local len = math.sqrt(dx*dx+dy*dy)
                    if len > 1 then
                        local nx, ny = -dy/len*r.w/2, dx/len*r.w/2
                        MAPX.poly_fill({{C.XY(r.x1+nx,r.y1+ny)}, {C.XY(r.x2+nx,r.y2+ny)},
                                        {C.XY(r.x2-nx,r.y2-ny)}, {C.XY(r.x1-nx,r.y1-ny)}}, MC.cross)
                    end
                end
            end
        end
    end
    end -- runway alert overlay
    if layer == 0 or layer == 1 then
    -- Reserve the host header and lower controls before original label placement.
    C.placed = {{x0,0,x1,25}, {x0,(600-bottom)/2,x1,300},
                {OANS_R[1]-2,OANS_R[2]-2,300,OANS_R[4]+2}}
    if mode == 'plan' and outside_plan then C.placed[#C.placed+1] = {4,34,110,61} end
    if #(alerts or {}) > 0 then
        C.placed[#C.placed+1] = mode == 'plan' and {25,136,275,164} or
            (mode == 'arc' and {80,114,220,162} or {80,78,220,126})
    end
    if boeing_full_compass then select_stand_names(G, simple_stand_names);MAPX.draw_tower(C,true);boeing_labels(C)
    else
        MAPX.draw_tower(C)
        select_stand_names(G, simple_stand_names)
        MAPX.labels(C)
    end
    end -- static labels and tower (separated from surfaces by the host)
    if layer == 0 or layer == 2 or layer == 3 then
    if boeing_full_compass and mode == 'arc' then boeing_arc_compass(C,mag)
    else NDUI.compass(C) end
    end
    if layer == 0 or layer == 2 or layer == 4 then
    local ownship_scale = 0.8
    for _, target in ipairs(traffic or {}) do
            local tx, ty = C.XY(G.W(target[1],target[2]))
            if tx > x0-20 and tx < x1+20 and ty > -20 and ty < 320 then
                draw_plane(tx,ty,(target[3]-C.hv)*D2R,MC.white,ownship_scale*0.8)
            end
    end
    if traffic_labels_enabled and math.abs(rng-0.1)<0.0001 then
        -- Dynamic-only rendering starts with an empty placed list. Reserve the
        -- same host controls, all traffic symbols and the valid ownship first.
        C.placed = {{x0,0,x1,25},{x0,C.map_bottom,x1,300},
            {OANS_R[1]-2,OANS_R[2]-2,300,OANS_R[4]+2}}
        if position_valid then
            local px,py=C.XY(G.W(lat,lon))
            C.placed[#C.placed+1]={px-9,py-9,px+9,py+9}
        end
        for _, target in ipairs(traffic or {}) do
            local x,y=C.XY(G.W(target[1],target[2]))
            C.placed[#C.placed+1]={x-7,y-7,x+7,y+7}
        end
        for _, target in ipairs(traffic or {}) do
            local x,y=C.XY(G.W(target[1],target[2]))
            if x>x0 and x<x1 and y>25 and y<C.map_bottom then traffic_label(C,target,x,y) end
        end
    end
    if position_valid then
        local px, py = C.XY(G.W(lat,lon))
        if px > x0-20 and px < x1+20 and py > -20 and py < 320 then
            if boeing_full_compass then boeing_ownship(px,py,(psi-C.hv)*D2R)
            else draw_plane(px,py,(psi-C.hv)*D2R,MC.acft,ownship_scale) end
        end
    end
    end -- dynamic layer: compass, ownship and traffic
    if boeing_full_compass then
        for k,v in pairs(saved_mc) do MC[k]=v end
        for _,key in ipairs({'stand_areas','road','wline','dash_y','tw_net','tl_m','tl'}) do G[key]=saved_geo[key] end
        G.buildings=saved_geo.buildings
    end
    return GLB.n, C.label_info, surface_vertices
end

-- Use A350's original B612 metrics and Airbus key primitives for annunciation.
-- This entry also works over the original ND/PFD without clearing its texture.
function a350_advisory(mode, pfd, hits, fms_runway)
    GLB.n, GLB.cap = 0, 180000
    native_draw_reset()
    GLB.wu, GLB.wv, GLB.px, GLB.ts = TF.wu, TF.wv, 1, 2
    DL.begin({2,0,0,0,-2,600,0,0}, 300, 300)
    FONT_H = TXT.h
    -- A320 display proportions, calibrated against the supplied manual ND.
    -- ARC centre: 42% of screen height; larger cap and 20-unit high boxes.
    local cap = pfd and 9.6 or 8.8
    local function same(a,b) return tostring(a):gsub('^0','') == tostring(b):gsub('^0','') end
    local function box(label, yc, xc, width)
        xc = xc or 150
        local w = width or text_w(label, cap)+10
        local half_height = pfd and 9 or 10
        local r = {xc-w/2,yc-half_height,xc+w/2,yc+half_height}
        DL.rectf(r[1],r[2],r[3],r[4],MC.bg)
        DL.rect(r[1],r[2],r[3],r[4],MC.cross,0,0,1.0)
        nd_key_text(r,MC.cross,label,cap)
        return r
    end
    if pfd then box('RWY AHEAD',128,132) -- manual: above horizon, centred on A320 attitude display
    elseif mode == 'plan' then box('RWY AHEAD: CHANGE MODE',150)
    else
        local yc = mode == 'arc' and 126 or 90
        local width = 0
        for _, hit in ipairs(hits) do
            width = math.max(width, text_w(hit[1] .. ' - ' .. hit[2], cap)+10)
        end
        for i, hit in ipairs(hits) do
            local label = hit[1] .. ' - ' .. hit[2]
            local r = box(label,yc+(i-1)*22,nil,width)
            local x = (r[1]+r[3])/2-text_w(label,cap)/2
            local top = (r[2]+r[4])/2-FONT_H/2
            if same(hit[1],fms_runway) then DL.text(x,top,MC.green,hit[1],cap) end
            if same(hit[2],fms_runway) then
                DL.text(x+text_w(hit[1]..' - ',cap),top,MC.green,hit[2],cap)
            end
        end
    end
    return GLB.n
end
-- ROSE markers need their own coverage fringe: ordinary filled triangles have
-- hard raster edges. Keep the nominal outline at 50% coverage, with a one-host-
-- pixel transition shared by the three edges (mitered corners, no overlaps).
function NDUI.rose_marker(x1,y1,x2,y2,x3,y3,col)
    if not DL.H then return DL.tri(x1,y1,x2,y2,x3,y3,col) end
    local points={{DL.map(x1,y1)},{DL.map(x2,y2)},{DL.map(x3,y3)}}
    local a,b,c=points[1],points[2],points[3]
    local area=(b[1]-a[1])*(c[2]-a[2])-(b[2]-a[2])*(c[1]-a[1])
    if math.abs(area)<1e-9 then return end
    if GLB.n+57>GLB.cap then return end
    local sign=area>0 and 1 or -1
    local normals,inner,outer={},{},{}
    for i,p in ipairs(points) do
        local q=points[i%3+1]
        local dx,dy=q[1]-p[1],q[2]-p[2]
        local len=math.sqrt(dx*dx+dy*dy)
        normals[i]={sign*dy/len,-sign*dx/len}
    end
    for i,p in ipairs(points) do
        local n,m=normals[(i+1)%3+1],normals[i]
        local f=GLB.px*0.5/(1+n[1]*m[1]+n[2]*m[2])
        local dx,dy=(n[1]+m[1])*f,(n[2]+m[2])*f
        inner[i],outer[i]={p[1]-dx,p[2]-dy},{p[1]+dx,p[2]+dy}
    end
    if GLB.fa then col=GLB.fade(col) end
    local clear=bit.band(col,0x00FFFFFF)
    native_draw_tri(inner[1][1],inner[1][2],inner[2][1],inner[2][2],
                    inner[3][1],inner[3][2],col,GLB.wu,GLB.wv)
    for i,p in ipairs(inner) do
        local j=i%3+1
        local q,u,v=inner[j],outer[i],outer[j]
        -- Reuse the native gradient strip; its core and second side collapse.
        native_draw_line(p[1],p[2],q[1],q[2],q[1],q[2],p[1],p[2],
                         u[1],u[2],v[1],v[2],p[1],p[2],q[1],q[2],
                         col,clear,GLB.wu,GLB.wv)
    end
    GLB.n,DL.n=GLB.n+57,DL.n+57
end
