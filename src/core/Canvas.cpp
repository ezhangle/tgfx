/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      https://opensource.org/licenses/BSD-3-Clause
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "tgfx/core/Canvas.h"
#include "core/MCStack.h"
#include "core/PathRef.h"
#include "core/Rasterizer.h"
#include "gpu/DrawingManager.h"
#include "gpu/ProxyProvider.h"
#include "gpu/ops/ClearOp.h"
#include "gpu/ops/FillRectOp.h"
#include "gpu/ops/RRectOp.h"
#include "gpu/ops/TriangulatingPathOp.h"
#include "gpu/processors/AARectEffect.h"
#include "gpu/processors/DeviceSpaceTextureEffect.h"
#include "gpu/processors/TextureEffect.h"
#include "gpu/proxies/RenderTargetProxy.h"
#include "tgfx/core/BlendMode.h"
#include "tgfx/core/Mask.h"
#include "tgfx/core/PathEffect.h"
#include "tgfx/core/TextBlob.h"
#include "tgfx/gpu/Surface.h"
#include "tgfx/utils/UTF.h"
#include "utils/MathExtra.h"
#include "utils/SimpleTextShaper.h"
#include "utils/StrokeKey.h"

namespace tgfx {
// https://chromium-review.googlesource.com/c/chromium/src/+/1099564/
static constexpr int AA_TESSELLATOR_MAX_VERB_COUNT = 100;
// A factor used to estimate the memory size of a tessellated path, based on the average value of
// Buffer.size() / Path.countPoints() from 4300+ tessellated path data.
static constexpr int AA_TESSELLATOR_BUFFER_SIZE_FACTOR = 170;

static bool ShouldTriangulatePath(const Path& path, const Matrix& viewMatrix) {
  if (path.countVerbs() <= AA_TESSELLATOR_MAX_VERB_COUNT) {
    return true;
  }
  auto scales = viewMatrix.getAxisScales();
  auto bounds = path.getBounds();
  bounds.scale(scales.x, scales.y);
  auto width = static_cast<int>(ceilf(bounds.width()));
  auto height = static_cast<int>(ceilf(bounds.height()));
  return path.countPoints() * AA_TESSELLATOR_BUFFER_SIZE_FACTOR <= width * height;
}

Canvas::Canvas(Surface* surface) : surface(surface) {
  Path clip = {};
  clip.addRect(0, 0, static_cast<float>(surface->width()), static_cast<float>(surface->height()));
  mcStack = new MCStack(clip);
}

Canvas::~Canvas() {
  delete mcStack;
}

void Canvas::save() {
  mcStack->save();
}

void Canvas::restore() {
  mcStack->restore();
}

void Canvas::translate(float dx, float dy) {
  mcStack->translate(dx, dy);
}

void Canvas::scale(float sx, float sy) {
  mcStack->scale(sx, sy);
}

void Canvas::rotate(float degrees) {
  mcStack->rotate(degrees);
}

void Canvas::rotate(float degress, float px, float py) {
  mcStack->rotate(degress, px, py);
}

void Canvas::skew(float sx, float sy) {
  mcStack->skew(sx, sy);
}

void Canvas::concat(const Matrix& matrix) {
  mcStack->concat(matrix);
}

Matrix Canvas::getMatrix() const {
  return mcStack->getMatrix();
}

void Canvas::setMatrix(const Matrix& matrix) {
  mcStack->setMatrix(matrix);
}

void Canvas::resetMatrix() {
  mcStack->resetMatrix();
}

Path Canvas::getTotalClip() const {
  return mcStack->getClip();
}

void Canvas::clipRect(const tgfx::Rect& rect) {
  mcStack->clipRect(rect);
}

void Canvas::clipPath(const Path& path) {
  mcStack->clipPath(path);
}

void Canvas::clear(const Color& color) {
  Paint paint;
  paint.setColor(color);
  paint.setBlendMode(BlendMode::Src);
  auto rect = Rect::MakeWH(surface->width(), surface->height());
  drawRect(rect, paint);
}

void Canvas::drawLine(float x0, float y0, float x1, float y1, const Paint& paint) {
  Path path = {};
  path.moveTo(x0, y0);
  path.lineTo(x1, y1);
  auto realPaint = paint;
  realPaint.setStyle(PaintStyle::Stroke);
  drawPath(path, realPaint);
}

void Canvas::drawRect(const Rect& rect, const Paint& paint) {
  Path path = {};
  path.addRect(rect);
  drawPath(path, paint);
}

void Canvas::drawOval(const Rect& oval, const Paint& paint) {
  Path path = {};
  path.addOval(oval);
  drawPath(path, paint);
}

void Canvas::drawCircle(float centerX, float centerY, float radius, const Paint& paint) {
  Rect rect =
      Rect::MakeLTRB(centerX - radius, centerY - radius, centerX + radius, centerY + radius);
  drawOval(rect, paint);
}

static Paint CleanPaint(const Paint* paint, bool forImage = false) {
  if (paint == nullptr) {
    return {};
  }
  auto cleaned = *paint;
  if (forImage) {
    cleaned.setStyle(PaintStyle::Fill);
  }
  if (auto shader = cleaned.getShader()) {
    Color shaderColor = {};
    if (shader->asColor(&shaderColor)) {
      shaderColor.alpha *= cleaned.getAlpha();
      cleaned.setColor(shaderColor);
      cleaned.setShader(nullptr);
    }
  }
  return cleaned;
}

Context* Canvas::getContext() const {
  return surface->getContext();
}

Surface* Canvas::getSurface() const {
  return surface;
}

const SurfaceOptions* Canvas::surfaceOptions() const {
  return surface->options();
}

/**
 * Defines the maximum distance a draw can extend beyond a clip's boundary and still be considered
 * 'on the other side'. This tolerance accounts for potential floating point rounding errors. The
 * value of 1e-3 is chosen because, in the coverage case, as long as coverage stays within
 * 0.5 * 1/256 of its intended value, it shouldn't affect the final pixel values.
 */
static constexpr float BOUNDS_TOLERANCE = 1e-3f;

/**
 * Returns true if the given rect counts as aligned with pixel boundaries.
 */
static bool IsPixelAligned(const Rect& rect) {
  return fabsf(roundf(rect.left) - rect.left) <= BOUNDS_TOLERANCE &&
         fabsf(roundf(rect.top) - rect.top) <= BOUNDS_TOLERANCE &&
         fabsf(roundf(rect.right) - rect.right) <= BOUNDS_TOLERANCE &&
         fabsf(roundf(rect.bottom) - rect.bottom) <= BOUNDS_TOLERANCE;
}

static void FlipYIfNeeded(Rect* rect, const Surface* surface) {
  if (surface->origin() == ImageOrigin::BottomLeft) {
    auto height = rect->height();
    rect->top = static_cast<float>(surface->height()) - rect->bottom;
    rect->bottom = rect->top + height;
  }
}

std::pair<std::optional<Rect>, bool> Canvas::getClipRect(const Rect* deviceBounds) {
  auto& clip = mcStack->getClip();
  auto rect = Rect::MakeEmpty();
  if (clip.asRect(&rect)) {
    if (deviceBounds != nullptr && !rect.intersect(*deviceBounds)) {
      return {{}, false};
    }
    FlipYIfNeeded(&rect, surface);
    if (IsPixelAligned(rect)) {
      rect.round();
      if (rect != Rect::MakeWH(surface->width(), surface->height())) {
        return {rect, true};
      } else {
        return {Rect::MakeEmpty(), false};
      }
    } else {
      return {rect, false};
    }
  }
  return {{}, false};
}

std::shared_ptr<TextureProxy> Canvas::getClipTexture() {
  auto& clip = mcStack->getClip();
  auto domainID = PathRef::GetUniqueKey(clip).domainID();
  if (domainID == clipID) {
    return clipTexture;
  }
  auto bounds = clip.getBounds();
  auto width = static_cast<int>(ceilf(bounds.width()));
  auto height = static_cast<int>(ceilf(bounds.height()));
  auto rasterizeMatrix = Matrix::MakeTrans(-bounds.left, -bounds.top);
  auto renderFlags = surfaceOptions()->renderFlags();
  if (ShouldTriangulatePath(clip, rasterizeMatrix)) {
    auto drawOp =
        TriangulatingPathOp::Make(Color::White(), clip, rasterizeMatrix, nullptr, renderFlags);
    auto renderTarget = RenderTargetProxy::Make(getContext(), width, height, PixelFormat::ALPHA_8);
    if (renderTarget == nullptr) {
      renderTarget = RenderTargetProxy::Make(getContext(), width, height, PixelFormat::RGBA_8888);
      if (renderTarget == nullptr) {
        return nullptr;
      }
    }
    auto drawingManager = getContext()->drawingManager();
    auto renderTask = drawingManager->addOpsTask(renderTarget);
    renderTask->addOp(std::move(drawOp));
    clipTexture = renderTarget->getTextureProxy();
  } else {
    auto uniqueKey = PathRef::GetUniqueKey(clip);
    auto rasterizer = Rasterizer::MakeFrom(clip, ISize::Make(width, height), rasterizeMatrix);
    auto proxyProvider = getContext()->proxyProvider();
    clipTexture = proxyProvider->createTextureProxy({}, rasterizer, false, renderFlags);
  }
  clipID = domainID;
  return clipTexture;
}

std::unique_ptr<FragmentProcessor> Canvas::getClipMask(const Rect& deviceBounds,
                                                       Rect* scissorRect) {
  auto& clip = mcStack->getClip();
  auto viewMatrix = mcStack->getMatrix();
  if (!clip.isEmpty() && clip.contains(deviceBounds)) {
    return nullptr;
  }
  auto [rect, useScissor] = getClipRect();
  if (rect.has_value()) {
    if (!rect->isEmpty()) {
      *scissorRect = *rect;
      if (!useScissor) {
        scissorRect->roundOut();
        return AARectEffect::Make(*rect);
      }
    }
    return nullptr;
  }
  auto clipBounds = clip.getBounds();
  *scissorRect = clipBounds;
  FlipYIfNeeded(scissorRect, surface);
  scissorRect->roundOut();
  auto texture = getClipTexture();
  if (texture == nullptr) {
    return nullptr;
  }
  auto localMatrix = viewMatrix;
  localMatrix.postTranslate(-clipBounds.left, -clipBounds.top);
  auto maskEffect = TextureEffect::Make(texture, {}, &localMatrix);
  if (!texture->isAlphaOnly()) {
    maskEffect = FragmentProcessor::MulInputByChildAlpha(std::move(maskEffect));
  }
  return maskEffect;
}

Rect Canvas::clipLocalBounds(const Rect& localBounds) {
  auto& viewMatrix = mcStack->getMatrix();
  Matrix invert = {};
  if (!viewMatrix.invert(&invert)) {
    return {};
  }
  auto drawRect = localBounds;
  auto clipBounds = mcStack->getClip().getBounds();
  invert.mapRect(&clipBounds);
  if (!drawRect.intersect(clipBounds)) {
    return {};
  }
  return drawRect;
}

static std::unique_ptr<DrawOp> MakeSimplePathOp(const Path& path, const DrawArgs& args) {
  Rect rect = {};
  if (path.asRect(&rect)) {
    return FillRectOp::Make(args.color, rect, args.viewMatrix);
  }
  RRect rRect;
  if (path.asRRect(&rRect)) {
    return RRectOp::Make(args.color, rRect, args.viewMatrix);
  }
  return nullptr;
}

static std::unique_ptr<DrawOp> MakeTexturePathOp(const Path& path, const DrawArgs& args,
                                                 const Stroke* stroke) {
  auto scales = args.viewMatrix.getAxisScales();
  auto bounds = path.getBounds();
  bounds.scale(scales.x, scales.y);
  static const auto TexturePathType = UniqueID::Next();
  BytesKey bytesKey(3 + (stroke ? StrokeKeyCount : 0));
  bytesKey.write(TexturePathType);
  bytesKey.write(scales.x);
  bytesKey.write(scales.y);
  if (stroke) {
    WriteStrokeKey(&bytesKey, stroke);
  }
  auto uniqueKey = UniqueKey::Combine(PathRef::GetUniqueKey(path), bytesKey);
  auto width = ceilf(bounds.width());
  auto height = ceilf(bounds.height());
  auto localMatrix = Matrix::MakeScale(scales.x, scales.y);
  localMatrix.postTranslate(-bounds.x(), -bounds.y());
  auto rasterizer = Rasterizer::MakeFrom(path, ISize::Make(width, height), localMatrix, stroke);
  auto proxyProvider = args.context->proxyProvider();
  auto textureProxy =
      proxyProvider->createTextureProxy(uniqueKey, rasterizer, false, args.renderFlags);
  if (textureProxy == nullptr) {
    return nullptr;
  }
  auto maskProcessor =
      TextureEffect::Make(std::move(textureProxy), SamplingOptions(), &localMatrix);
  if (maskProcessor == nullptr) {
    return nullptr;
  }
  auto op = FillRectOp::Make(args.color, args.drawRect, args.viewMatrix);
  op->addColorFP(std::move(maskProcessor));
  return op;
}

static Path GetSimpleFillPath(const Path& path, const Paint& paint) {
  if (paint.getStyle() == PaintStyle::Fill) {
    return path;
  }
  if (path.isLine()) {
    auto effect = PathEffect::MakeStroke(paint.getStroke());
    if (effect != nullptr) {
      auto tempPath = path;
      effect->applyTo(&tempPath);
      return tempPath;
    }
  }
  return {};
}

void Canvas::drawPath(const Path& path, const Paint& paint) {
  if (path.isEmpty() || paint.nothingToDraw()) {
    return;
  }
  auto realPaint = CleanPaint(&paint);
  auto stroke = realPaint.getStroke();
  auto pathBounds = path.getBounds();
  if (stroke != nullptr) {
    pathBounds.outset(stroke->width, stroke->width);
  }
  auto localBounds = clipLocalBounds(pathBounds);
  if (localBounds.isEmpty()) {
    return;
  }
  auto fillPath = GetSimpleFillPath(path, realPaint);
  if (drawAsClear(fillPath, realPaint)) {
    return;
  }
  auto viewMatrix = mcStack->getMatrix();
  DrawArgs args(surface, realPaint, localBounds, viewMatrix);
  auto drawOp = MakeSimplePathOp(fillPath, args);
  if (drawOp != nullptr) {
    addDrawOp(std::move(drawOp), args, realPaint);
    return;
  }
  if (ShouldTriangulatePath(path, viewMatrix)) {
    drawOp = TriangulatingPathOp::Make(args.color, path, viewMatrix, stroke, args.renderFlags);
  } else {
    drawOp = MakeTexturePathOp(path, args, stroke);
  }
  addDrawOp(std::move(drawOp), args, realPaint);
}

void Canvas::drawImage(std::shared_ptr<Image> image, float left, float top, const Paint* paint) {
  drawImage(std::move(image), Matrix::MakeTrans(left, top), paint);
}

void Canvas::drawImage(std::shared_ptr<Image> image, const Matrix& matrix, const Paint* paint) {
  auto oldMatrix = getMatrix();
  concat(matrix);
  drawImage(std::move(image), paint);
  setMatrix(oldMatrix);
}

void Canvas::drawImage(std::shared_ptr<Image> image, const Paint* paint) {
  if (image == nullptr) {
    return;
  }
  auto mipmapMode = image->hasMipmaps() ? MipmapMode::Linear : MipmapMode::None;
  SamplingOptions sampling(FilterMode::Linear, mipmapMode);
  drawImage(std::move(image), sampling, paint);
}

void Canvas::drawImage(std::shared_ptr<Image> image, SamplingOptions sampling, const Paint* paint) {
  if (image == nullptr) {
    return;
  }
  auto realPaint = CleanPaint(paint, true);
  if (realPaint.nothingToDraw()) {
    return;
  }
  auto oldMatrix = getMatrix();
  auto imageFilter = realPaint.getImageFilter();
  if (imageFilter != nullptr) {
    auto offset = Point::Zero();
    image = image->makeWithFilter(std::move(imageFilter), &offset);
    if (image == nullptr) {
      return;
    }
    realPaint.setImageFilter(nullptr);
    concat(Matrix::MakeTrans(offset.x, offset.y));
  }
  auto localBounds = clipLocalBounds(Rect::MakeWH(image->width(), image->height()));
  if (localBounds.isEmpty()) {
    return;
  }
  if (realPaint.getShader() != nullptr && !image->isAlphaOnly()) {
    realPaint.setShader(nullptr);
  }
  DrawArgs args(surface, realPaint, localBounds, mcStack->getMatrix());
  auto processor = FragmentProcessor::Make(std::move(image), args, sampling);
  if (processor == nullptr) {
    return;
  }
  auto drawOp = FillRectOp::Make(args.color, args.drawRect, args.viewMatrix);
  drawOp->addColorFP(std::move(processor));
  addDrawOp(std::move(drawOp), args, realPaint);
  setMatrix(oldMatrix);
}

void Canvas::drawMask(const Rect& deviceBounds, std::shared_ptr<TextureProxy> textureProxy,
                      const Paint& paint) {
  if (textureProxy == nullptr) {
    return;
  }
  auto localMatrix = Matrix::I();
  auto maskLocalMatrix = Matrix::I();
  auto& viewMatrix = mcStack->getMatrix();
  if (!viewMatrix.invert(&localMatrix)) {
    return;
  }
  maskLocalMatrix.postConcat(viewMatrix);
  maskLocalMatrix.postTranslate(-deviceBounds.x(), -deviceBounds.y());
  maskLocalMatrix.postScale(static_cast<float>(textureProxy->width()) / deviceBounds.width(),
                            static_cast<float>(textureProxy->height()) / deviceBounds.height());
  resetMatrix();
  DrawArgs args(surface, paint, deviceBounds, Matrix::I());
  auto op = FillRectOp::Make(args.color, args.drawRect, args.viewMatrix, &localMatrix);
  auto maskProcessor = FragmentProcessor::MulInputByChildAlpha(
      TextureEffect::Make(std::move(textureProxy), SamplingOptions(), &maskLocalMatrix));
  if (maskProcessor == nullptr) {
    return;
  }
  op->addCoverageFP(std::move(maskProcessor));
  addDrawOp(std::move(op), args, paint);
  setMatrix(viewMatrix);
}

void Canvas::drawSimpleText(const std::string& text, float x, float y, const tgfx::Font& font,
                            const tgfx::Paint& paint) {
  auto [glyphIDs, positions] = SimpleTextShaper::Shape(text, font);
  if (x != 0 && y != 0) {
    for (auto& position : positions) {
      position.offset(x, y);
    }
  }
  drawGlyphs(glyphIDs.data(), positions.data(), glyphIDs.size(), font, paint);
}

void Canvas::drawGlyphs(const GlyphID glyphs[], const Point positions[], size_t glyphCount,
                        const Font& font, const Paint& paint) {
  if (glyphCount == 0 || paint.nothingToDraw()) {
    return;
  }
  auto realPaint = CleanPaint(&paint);
  auto scale = mcStack->getMatrix().getMaxScale();
  auto scaledFont = font.makeWithSize(font.getSize() * scale);
  realPaint.setStrokeWidth(realPaint.getStrokeWidth() * scale);
  std::vector<Point> scaledPositions;
  for (size_t i = 0; i < glyphCount; ++i) {
    scaledPositions.push_back(Point::Make(positions[i].x * scale, positions[i].y * scale));
  }
  save();
  concat(Matrix::MakeScale(1.f / scale));
  if (scaledFont.getTypeface()->hasColor()) {
    drawColorGlyphs(glyphs, scaledPositions.data(), glyphCount, scaledFont, realPaint);
    restore();
    return;
  }
  auto textBlob = TextBlob::MakeFrom(glyphs, scaledPositions.data(), glyphCount, scaledFont);
  if (textBlob) {
    drawMaskGlyphs(textBlob, realPaint);
  }
  restore();
}

void Canvas::drawColorGlyphs(const GlyphID glyphIDs[], const Point positions[], size_t glyphCount,
                             const Font& font, const Paint& paint) {
  for (size_t i = 0; i < glyphCount; ++i) {
    const auto& glyphID = glyphIDs[i];
    const auto& position = positions[i];

    auto glyphMatrix = Matrix::I();
    auto glyphBuffer = font.getImage(glyphID, &glyphMatrix);
    if (glyphBuffer == nullptr) {
      continue;
    }
    glyphMatrix.postTranslate(position.x, position.y);
    save();
    concat(glyphMatrix);
    auto image = Image::MakeFrom(glyphBuffer);
    drawImage(std::move(image), &paint);
    restore();
  }
}

void Canvas::drawMaskGlyphs(std::shared_ptr<TextBlob> textBlob, const Paint& paint) {
  if (textBlob == nullptr) {
    return;
  }
  auto stroke = paint.getStroke();
  auto localBounds = clipLocalBounds(textBlob->getBounds(stroke));
  if (localBounds.isEmpty()) {
    return;
  }
  auto& viewMatrix = mcStack->getMatrix();
  auto deviceBounds = viewMatrix.mapRect(localBounds);
  auto width = ceilf(deviceBounds.width());
  auto height = ceilf(deviceBounds.height());
  auto totalMatrix = viewMatrix;
  auto matrix = Matrix::I();
  matrix.postTranslate(-deviceBounds.x(), -deviceBounds.y());
  matrix.postScale(width / deviceBounds.width(), height / deviceBounds.height());
  totalMatrix.postConcat(matrix);
  auto rasterizer = Rasterizer::MakeFrom(textBlob, ISize::Make(width, height), totalMatrix, stroke);
  auto textureProxy = getContext()->proxyProvider()->createTextureProxy(
      {}, std::move(rasterizer), false, surface->options()->renderFlags());
  drawMask(deviceBounds, std::move(textureProxy), paint);
}

void Canvas::drawAtlas(std::shared_ptr<Image> atlas, const Matrix matrix[], const Rect tex[],
                       const Color colors[], size_t count, SamplingOptions sampling,
                       const Paint* paint) {
  // TODO: Support blend mode, atlas as source, colors as destination, colors can be nullptr.
  if (atlas == nullptr || count == 0) {
    return;
  }
  auto totalMatrix = mcStack->getMatrix();
  auto realPaint = CleanPaint(paint, true);
  for (size_t i = 0; i < count; ++i) {
    concat(matrix[i]);
    auto width = static_cast<float>(tex[i].width());
    auto height = static_cast<float>(tex[i].height());
    auto localBounds = clipLocalBounds(Rect::MakeWH(width, height));
    if (localBounds.isEmpty()) {
      setMatrix(totalMatrix);
      continue;
    }
    auto localMatrix = Matrix::MakeTrans(tex[i].x(), tex[i].y());
    auto color = colors ? std::optional<Color>(colors[i].premultiply()) : std::nullopt;
    auto& viewMatrix = mcStack->getMatrix();
    auto drawOp = FillRectOp::Make(color, localBounds, viewMatrix, &localMatrix);
    DrawArgs args(surface, realPaint, localBounds, totalMatrix);
    auto processor = FragmentProcessor::Make(atlas, args, sampling);
    if (processor == nullptr) {
      return;
    }
    drawOp->addColorFP(std::move(processor));
    addDrawOp(std::move(drawOp), args, realPaint);
    setMatrix(totalMatrix);
  }
}

static bool HasColorOnly(const Paint& paint) {
  return paint.getColorFilter() == nullptr && paint.getShader() == nullptr &&
         paint.getImageFilter() == nullptr && paint.getMaskFilter() == nullptr;
}

bool Canvas::drawAsClear(const Path& path, const Paint& paint) {
  if (!HasColorOnly(paint) || !mcStack->getMatrix().rectStaysRect()) {
    return false;
  }
  auto color = paint.getColor().premultiply();
  auto blendMode = paint.getBlendMode();
  if (blendMode == BlendMode::Clear) {
    color = Color::Transparent();
  } else if (blendMode != BlendMode::Src) {
    if (!color.isOpaque()) {
      return false;
    }
  }
  auto bounds = Rect::MakeEmpty();
  if (!path.asRect(&bounds)) {
    return false;
  }
  mcStack->getMatrix().mapRect(&bounds);
  auto [clipRect, useScissor] = getClipRect(&bounds);
  if (clipRect.has_value()) {
    auto format = surface->renderTargetProxy->format();
    const auto& writeSwizzle = getContext()->caps()->getWriteSwizzle(format);
    color = writeSwizzle.applyTo(color);
    if (useScissor) {
      surface->aboutToDraw();
      surface->addOp(ClearOp::Make(color, *clipRect));
      return true;
    } else if (clipRect->isEmpty()) {
      surface->aboutToDraw(true);
      surface->addOp(ClearOp::Make(color, bounds));
      return true;
    }
  }
  return false;
}

bool Canvas::getProcessors(const DrawArgs& args, const Paint& paint, DrawOp* drawOp) {
  if (drawOp == nullptr) {
    return false;
  }
  if (auto shader = paint.getShader()) {
    auto shaderFP = FragmentProcessor::Make(shader, args);
    if (shaderFP == nullptr) {
      return false;
    }
    drawOp->addColorFP(std::move(shaderFP));
  }
  if (auto colorFilter = paint.getColorFilter()) {
    if (auto processor = colorFilter->asFragmentProcessor()) {
      drawOp->addColorFP(std::move(processor));
    } else {
      return false;
    }
  }
  if (auto maskFilter = paint.getMaskFilter()) {
    if (auto processor = maskFilter->asFragmentProcessor(args, nullptr)) {
      drawOp->addCoverageFP(std::move(processor));
    } else {
      return false;
    }
  }
  return true;
}

void Canvas::addDrawOp(std::unique_ptr<DrawOp> op, const DrawArgs& args, const Paint& paint) {
  if (!getProcessors(args, paint, op.get())) {
    return;
  }
  auto aaType = AAType::None;
  if (surface->renderTargetProxy->sampleCount() > 1) {
    aaType = AAType::MSAA;
  } else if (paint.isAntiAlias()) {
    auto isFillRect = op->classID() == FillRectOp::ClassID();
    if (!isFillRect || !args.viewMatrix.rectStaysRect() || !IsPixelAligned(op->bounds())) {
      aaType = AAType::Coverage;
    }
  }
  op->setAA(aaType);
  op->setBlendMode(paint.getBlendMode());
  Rect scissorRect = Rect::MakeEmpty();
  auto clipMask = getClipMask(op->bounds(), &scissorRect);
  if (clipMask) {
    op->addCoverageFP(std::move(clipMask));
  }
  op->setScissorRect(scissorRect);
  surface->aboutToDraw(false);
  surface->addOp(std::move(op));
}
}  // namespace tgfx
