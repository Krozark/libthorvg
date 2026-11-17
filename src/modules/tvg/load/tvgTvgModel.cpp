/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "tvgCompressor.h"
#include "tvgTvgModel.h"
#include "tvgShape.h"


/************************************************************************/
/* TvgTransform                                                         */
/************************************************************************/

void TvgTransform::matrix(float progress, Matrix& out) const
{
    // T(translate) * R(rotation) * K(skew) * S(scale) * T(-anchor)
    auto r = deg2rad(this->r(progress));
    auto cv = cosf(r);
    auto sv = sinf(r);

    auto k = this->k(progress);
    auto kx = tanf(deg2rad(k.x));
    auto ky = tanf(deg2rad(k.y));

    auto s = this->s(progress);

    out = {(cv - sv * ky) * s, (cv * kx - sv) * s, 0.0f,
           (sv + cv * ky) * s, (sv * kx + cv) * s, 0.0f,
           0.0f, 0.0f, 1.0f};

    auto t = this->t(progress);
    auto a = this->a(progress);

    out.e13 = t.x - out.e11 * a.x - out.e12 * a.y;
    out.e23 = t.y - out.e21 * a.x - out.e22 * a.y;
}

/************************************************************************/
/* TvgPaint                                                             */
/************************************************************************/

TvgPaint::TvgPaint(TvgPaint* parent, Paint* obj, const char* id) : obj(obj)
{
    if (parent) static_cast<TvgScene*>(parent)->scene()->add(obj);
    obj->ref();
    obj->id = djb2Encode(id);
    obj->data = this;
}

TvgPaint::~TvgPaint()
{
    delete (t);
    obj->unref();
}

bool TvgPaint::build(float progress, bool force)
{
    if (!force && statical) return false;

    obj->opacity(o(progress));
    if (t) {
        t->matrix(progress, obj->transform());
        obj->transform(obj->transform());
    }
    return true;
}

/************************************************************************/
/* TvgScene                                                             */
/************************************************************************/

TvgScene::~TvgScene()
{
    for (auto c : scene()->paints()) {
        delete ((TvgPaint*)(c->data));
    }
}

bool TvgScene::build(float progress, bool force)
{
    auto obj = scene();

    // out of frame
    if (progress < i[0] || progress >= i[1]) {
        obj->visible(false);
        return false;
    }

    auto remap = map(progress);
    if (!TvgPaint::build(remap, force)) return false;

    for (auto c : obj->paints()) {
        switch (c->type()) {
            case Type::Scene: static_cast<TvgScene*>(c->data)->build(progress); break;
            case Type::Shape: static_cast<TvgShape*>(c->data)->build(remap); break;
            case Type::Picture: break;
            case Type::Text: break;
            default: break;
        }
    }

    obj->visible(true);

    return true;
}

/************************************************************************/
/* TvgFill                                                              */
/************************************************************************/

/************************************************************************/
/* TvgShape                                                             */
/************************************************************************/

void TvgShape::Rect::dispatch(float progress, Shape* out)
{
    auto p = this->p(progress);
    auto z = this->z(progress);
    auto r = this->r(progress);
    out->appendRect(p.x, p.y, z.x, z.y, r.x, r.y, d);
}

void TvgShape::Circle::dispatch(float progress, Shape* out)
{
    auto c = this->c(progress);
    auto r = this->r(progress);
    out->appendCircle(c.x, c.y, r.x, r.y, d);
}

TvgShape::Custom::Pts::~Pts()
{
    if (pts.cnt == 0) tvg::free(pts.value);
    for (uint32_t i = 0; i < pts.cnt; ++i) {
        tvg::free(pts.frames[i].value);
    }
}

void TvgShape::Custom::dispatch(float progress, Shape* out)
{
    // a trick approach for passing memory copy
    auto& path = to<ShapeImpl>(out)->rs.path;
    to<ShapeImpl>(out)->impl.mark(RenderUpdateFlag::Path);
    // append points directly
    pts.pts(progress, path.pts, pts.cnt);
    // append commands directly
    path.cmds.grow(cmd.cnt);
    memcpy(path.cmds.end(), cmd.cmds, sizeof(PathCommand) * cmd.cnt);
    path.cmds.count += cmd.cnt;
}

TvgShape::~TvgShape()
{
    for (auto p : list) delete p;
    delete (g);
}

bool TvgShape::build(float progress, bool force)
{
    if (!TvgPaint::build(progress, force)) return false;

    auto obj = shape();
    obj->reset();

    // gradient fill
    if (g) {
        obj->fill(g->dispatch(progress, const_cast<Fill*>(obj->fill())));
    // solid fill
    } else {
        auto f = this->f(progress);
        obj->fill(f.r, f.g, f.b, f.a);
    }

    // paths
    for (auto path : list) {
        path->dispatch(progress, obj);
    }

    // stroke
    if (s) {
        obj->strokeWidth(s->w(progress));
        auto f = s->f(progress);
        obj->strokeFill(f.r, f.g, f.b, f.a);
    }

    // trimpath
    if (t) obj->trimpath(t->b(progress), t->e(progress), t->m);

    return true;
}

TvgShape::Stroke* TvgShape::stroke()
{
    if (!s) s = new Stroke;
    return s;
}

TvgShape::Trim* TvgShape::trim()
{
    if (!t) t = new Trim;
    return t;
}
