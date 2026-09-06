#include "LiveStitchPreviewBuilder.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <utility>

namespace {

struct Bounds {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    bool initialized = false;
};

bool HasReadablePixels(const ImageFrame& frame)
{
    return frame.IsValid() &&
        frame.stride >= frame.width * 3 &&
        frame.bgr.size() >=
            static_cast<std::size_t>(frame.stride) * static_cast<std::size_t>(frame.height);
}

bool HasReadablePixels(const LiveStitchPreviewTile& tile)
{
    return tile.frame && HasReadablePixels(*tile.frame);
}

void ExpandBounds(const StitchTile& tile, Bounds& bounds)
{
    if (!HasReadablePixels(tile.frame)) {
        return;
    }

    const int left = tile.offset_x;
    const int top = tile.offset_y;
    const int right = tile.offset_x + tile.frame.width;
    const int bottom = tile.offset_y + tile.frame.height;
    if (!bounds.initialized) {
        bounds = Bounds{left, top, right, bottom, true};
        return;
    }

    bounds.left = std::min(bounds.left, left);
    bounds.top = std::min(bounds.top, top);
    bounds.right = std::max(bounds.right, right);
    bounds.bottom = std::max(bounds.bottom, bottom);
}

void ExpandBounds(const LiveStitchPreviewTile& tile, Bounds& bounds)
{
    if (!HasReadablePixels(tile)) {
        return;
    }

    const int left = tile.offset_x;
    const int top = tile.offset_y;
    const int right = tile.offset_x + tile.frame->width;
    const int bottom = tile.offset_y + tile.frame->height;
    if (!bounds.initialized) {
        bounds = Bounds{left, top, right, bottom, true};
        return;
    }

    bounds.left = std::min(bounds.left, left);
    bounds.top = std::min(bounds.top, top);
    bounds.right = std::max(bounds.right, right);
    bounds.bottom = std::max(bounds.bottom, bottom);
}

int PreviewScaleFor(const std::vector<StitchTile>& tiles, int max_preview_edge)
{
    Bounds bounds;
    int max_tile_edge = 1;
    for (const StitchTile& tile : tiles) {
        if (!HasReadablePixels(tile.frame)) {
            continue;
        }
        ExpandBounds(tile, bounds);
        max_tile_edge = std::max(max_tile_edge, std::max(tile.frame.width, tile.frame.height));
    }
    if (!bounds.initialized) {
        return 1;
    }

    const int mosaic_edge = std::max(bounds.right - bounds.left, bounds.bottom - bounds.top);
    const int max_edge = std::max(1, std::max(mosaic_edge, max_tile_edge));
    const int capped_edge = std::max(256, max_preview_edge);
    return std::max(1, (max_edge + capped_edge - 1) / capped_edge);
}

int PreviewScaleFor(const std::vector<LiveStitchPreviewTile>& tiles, int max_preview_edge)
{
    Bounds bounds;
    int max_tile_edge = 1;
    for (const LiveStitchPreviewTile& tile : tiles) {
        if (!HasReadablePixels(tile)) {
            continue;
        }
        ExpandBounds(tile, bounds);
        max_tile_edge = std::max(max_tile_edge, std::max(tile.frame->width, tile.frame->height));
    }
    if (!bounds.initialized) {
        return 1;
    }

    const int mosaic_edge = std::max(bounds.right - bounds.left, bounds.bottom - bounds.top);
    const int max_edge = std::max(1, std::max(mosaic_edge, max_tile_edge));
    const int capped_edge = std::max(256, max_preview_edge);
    return std::max(1, (max_edge + capped_edge - 1) / capped_edge);
}

ImageFrame DownsampleAverage(const ImageFrame& source, int scale)
{
    if (!HasReadablePixels(source) || scale <= 1) {
        return source;
    }

    ImageFrame preview;
    preview.width = std::max(1, (source.width + scale - 1) / scale);
    preview.height = std::max(1, (source.height + scale - 1) / scale);
    preview.stride = (preview.width * 3 + 3) & ~3;
    preview.timestamp = source.timestamp;
    preview.sequence = source.sequence;
    preview.bgr.assign(
        static_cast<std::size_t>(preview.stride) * static_cast<std::size_t>(preview.height),
        0);

    for (int y = 0; y < preview.height; ++y) {
        const int source_y0 = y * scale;
        const int source_y1 = std::min(source.height, source_y0 + scale);
        unsigned char* preview_row =
            preview.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(preview.stride);
        for (int x = 0; x < preview.width; ++x) {
            const int source_x0 = x * scale;
            const int source_x1 = std::min(source.width, source_x0 + scale);
            int blue_sum = 0;
            int green_sum = 0;
            int red_sum = 0;
            int samples = 0;
            for (int source_y = source_y0; source_y < source_y1; ++source_y) {
                const unsigned char* source_row =
                    source.bgr.data() + static_cast<std::size_t>(source_y) * static_cast<std::size_t>(source.stride);
                for (int source_x = source_x0; source_x < source_x1; ++source_x) {
                    const unsigned char* source_pixel = source_row + static_cast<std::size_t>(source_x) * 3U;
                    blue_sum += source_pixel[0];
                    green_sum += source_pixel[1];
                    red_sum += source_pixel[2];
                    ++samples;
                }
            }
            unsigned char* preview_pixel = preview_row + static_cast<std::size_t>(x) * 3U;
            preview_pixel[0] = static_cast<unsigned char>(blue_sum / std::max(1, samples));
            preview_pixel[1] = static_cast<unsigned char>(green_sum / std::max(1, samples));
            preview_pixel[2] = static_cast<unsigned char>(red_sum / std::max(1, samples));
        }
    }

    return preview;
}

std::vector<StitchTile> BuildPreviewTiles(const std::vector<StitchTile>& tiles, int scale)
{
    std::vector<StitchTile> preview_tiles;
    preview_tiles.reserve(tiles.size());
    for (const StitchTile& tile : tiles) {
        if (!HasReadablePixels(tile.frame)) {
            continue;
        }

        StitchTile preview_tile;
        preview_tile.frame = DownsampleAverage(tile.frame, scale);
        preview_tile.offset_x =
            static_cast<int>(std::lround(static_cast<double>(tile.offset_x) / static_cast<double>(scale)));
        preview_tile.offset_y =
            static_cast<int>(std::lround(static_cast<double>(tile.offset_y) / static_cast<double>(scale)));
        preview_tile.estimated_position = tile.estimated_position;
        preview_tiles.push_back(std::move(preview_tile));
    }
    return preview_tiles;
}

std::vector<StitchTile> BuildPreviewTiles(const std::vector<LiveStitchPreviewTile>& tiles, int scale)
{
    std::vector<StitchTile> preview_tiles;
    preview_tiles.reserve(tiles.size());
    for (const LiveStitchPreviewTile& tile : tiles) {
        if (!HasReadablePixels(tile)) {
            continue;
        }

        StitchTile preview_tile;
        preview_tile.frame = DownsampleAverage(*tile.frame, scale);
        preview_tile.offset_x =
            static_cast<int>(std::lround(static_cast<double>(tile.offset_x) / static_cast<double>(scale)));
        preview_tile.offset_y =
            static_cast<int>(std::lround(static_cast<double>(tile.offset_y) / static_cast<double>(scale)));
        preview_tile.estimated_position = tile.estimated_position;
        preview_tiles.push_back(std::move(preview_tile));
    }
    return preview_tiles;
}

StitchResultMetadata BuildPreviewMetadata(
    const std::vector<StitchTile>& preview_tiles,
    const LiveStitchPreviewOptions& options,
    int scale)
{
    StitchResultMetadata metadata;
    metadata.available = !preview_tiles.empty();
    metadata.backend = scale > 1
        ? L"Live preview (downsampled)"
        : L"Live preview";
    metadata.layout_mode = L"Manual stage live capture";
    metadata.registration_method = L"Fast adjacent tile placement";
    metadata.transform_model = L"Translation preview";
    metadata.blend_mode = options.blend_mode == StitchBlendMode::None
        ? L"None"
        : L"Linear";
    metadata.overlap_percent = options.overlap_percent;
    metadata.grid_rows = 1;
    metadata.grid_cols = static_cast<int>(preview_tiles.size());
    metadata.relation_count = preview_tiles.empty()
        ? 0
        : static_cast<int>(preview_tiles.size() - 1U);
    metadata.tiles.reserve(preview_tiles.size());
    for (const StitchTile& tile : preview_tiles) {
        metadata.tiles.push_back(StitchResultTileMetadata{
            tile.offset_x,
            tile.offset_y,
            tile.frame.width,
            tile.frame.height,
            tile.estimated_position});
    }
    return metadata;
}

StitchResultMetadata BuildPreviewMetadata(
    const std::vector<LiveStitchPreviewTile>& preview_tiles,
    const LiveStitchPreviewOptions& options,
    int scale)
{
    StitchResultMetadata metadata;
    metadata.available = !preview_tiles.empty();
    metadata.backend = scale > 1
        ? L"Live preview (downsampled)"
        : L"Live preview";
    metadata.layout_mode = L"Manual stage live capture";
    metadata.registration_method = L"Fast adjacent tile placement";
    metadata.transform_model = L"Translation preview";
    metadata.blend_mode = options.blend_mode == StitchBlendMode::None
        ? L"None"
        : L"Linear";
    metadata.overlap_percent = options.overlap_percent;
    metadata.grid_rows = 1;
    metadata.grid_cols = static_cast<int>(preview_tiles.size());
    metadata.relation_count = preview_tiles.empty()
        ? 0
        : static_cast<int>(preview_tiles.size() - 1U);
    metadata.tiles.reserve(preview_tiles.size());
    for (const LiveStitchPreviewTile& tile : preview_tiles) {
        if (!HasReadablePixels(tile)) {
            continue;
        }
        metadata.tiles.push_back(StitchResultTileMetadata{
            tile.offset_x,
            tile.offset_y,
            tile.frame->width,
            tile.frame->height,
            tile.estimated_position});
    }
    return metadata;
}

double PreviewLinearBlendWeight(const ImageFrame& frame, int x, int y)
{
    const int edge_distance = std::min({
        x + 1,
        y + 1,
        frame.width - x,
        frame.height - y});
    const int ramp = std::max(8, std::min(frame.width, frame.height) / 8);
    return std::clamp(static_cast<double>(edge_distance) / static_cast<double>(ramp), 0.04, 1.0);
}

ImageFrame StitchPreviewTiles(
    const std::vector<LiveStitchPreviewTile>& tiles,
    StitchBlendMode blend_mode)
{
    Bounds bounds;
    bool has_tile = false;
    for (const LiveStitchPreviewTile& tile : tiles) {
        if (HasReadablePixels(tile)) {
            ExpandBounds(tile, bounds);
            has_tile = true;
        }
    }
    if (!has_tile || bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
        return {};
    }

    ImageFrame output;
    output.width = bounds.right - bounds.left;
    output.height = bounds.bottom - bounds.top;
    output.stride = (output.width * 3 + 3) & ~3;
    output.bgr.assign(
        static_cast<std::size_t>(output.stride) * static_cast<std::size_t>(output.height),
        0);

    std::vector<std::size_t> readable_indices;
    readable_indices.reserve(tiles.size());
    for (std::size_t index = 0; index < tiles.size(); ++index) {
        if (HasReadablePixels(tiles[index])) {
            readable_indices.push_back(index);
        }
    }

    std::vector<std::size_t> active_indices;
    active_indices.reserve(readable_indices.size());
    for (int y = 0; y < output.height; ++y) {
        unsigned char* output_row =
            output.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(output.stride);
        const int canvas_y = bounds.top + y;
        active_indices.clear();
        for (std::size_t tile_index : readable_indices) {
            const LiveStitchPreviewTile& tile = tiles[tile_index];
            if (canvas_y >= tile.offset_y && canvas_y < tile.offset_y + tile.frame->height) {
                active_indices.push_back(tile_index);
            }
        }

        for (int x = 0; x < output.width; ++x) {
            const int canvas_x = bounds.left + x;
            double blue_sum = 0.0;
            double green_sum = 0.0;
            double red_sum = 0.0;
            double weight_sum = 0.0;

            for (std::size_t tile_index : active_indices) {
                const LiveStitchPreviewTile& tile = tiles[tile_index];
                const ImageFrame& frame = *tile.frame;
                const int tile_x = canvas_x - tile.offset_x;
                const int tile_y = canvas_y - tile.offset_y;
                if (tile_x < 0 || tile_y < 0 || tile_x >= frame.width || tile_y >= frame.height) {
                    continue;
                }

                const double weight = blend_mode == StitchBlendMode::Linear
                    ? PreviewLinearBlendWeight(frame, tile_x, tile_y)
                    : 1.0;
                if (weight <= 0.0) {
                    continue;
                }

                const unsigned char* source =
                    frame.bgr.data() +
                    static_cast<std::size_t>(tile_y) * static_cast<std::size_t>(frame.stride) +
                    static_cast<std::size_t>(tile_x) * 3U;
                blue_sum += static_cast<double>(source[0]) * weight;
                green_sum += static_cast<double>(source[1]) * weight;
                red_sum += static_cast<double>(source[2]) * weight;
                weight_sum += weight;
            }

            if (weight_sum <= 0.0) {
                continue;
            }

            output_row[x * 3 + 0] = static_cast<unsigned char>(
                std::lround(std::clamp(blue_sum / weight_sum, 0.0, 255.0)));
            output_row[x * 3 + 1] = static_cast<unsigned char>(
                std::lround(std::clamp(green_sum / weight_sum, 0.0, 255.0)));
            output_row[x * 3 + 2] = static_cast<unsigned char>(
                std::lround(std::clamp(red_sum / weight_sum, 0.0, 255.0)));
        }
    }

    return output;
}

std::vector<LiveStitchPreviewTile> BuildTileRefs(const std::vector<StitchTile>& tiles)
{
    std::vector<LiveStitchPreviewTile> refs;
    refs.reserve(tiles.size());
    for (const StitchTile& tile : tiles) {
        if (!HasReadablePixels(tile.frame)) {
            continue;
        }

        LiveStitchPreviewTile ref;
        ref.frame = std::make_shared<ImageFrame>(tile.frame);
        ref.offset_x = tile.offset_x;
        ref.offset_y = tile.offset_y;
        ref.estimated_position = tile.estimated_position;
        refs.push_back(std::move(ref));
    }
    return refs;
}

} // namespace

int LiveStitchPreviewBuilder::DownsampleScaleFor(const ImageFrame& source, int max_edge)
{
    if (!HasReadablePixels(source)) return 1;
    const int capped_edge = std::max(64, max_edge);
    const int source_edge = std::max(source.width, source.height);
    return std::max(1, (source_edge + capped_edge - 1) / capped_edge);
}

ImageFrame LiveStitchPreviewBuilder::DownsampleFrame(const ImageFrame& source, int scale)
{
    return DownsampleAverage(source, std::max(1, scale));
}

LiveStitchPreviewResult LiveStitchPreviewBuilder::Build(
    const std::vector<LiveStitchPreviewTile>& tiles,
    LiveStitchPreviewOptions options)
{
    LiveStitchPreviewResult result;
    const int scale = PreviewScaleFor(tiles, options.max_preview_edge);
    if (scale > 1) {
        std::vector<StitchTile> preview_tiles = BuildPreviewTiles(tiles, scale);
        if (preview_tiles.empty()) {
            return result;
        }
        result.scale = scale;
        result.image = ImageStitcher::StitchAverage(preview_tiles, options.blend_mode);
        result.metadata = BuildPreviewMetadata(preview_tiles, options, scale);
        return result;
    }

    if (tiles.empty()) {
        return result;
    }
    result.scale = 1;
    result.image = StitchPreviewTiles(tiles, options.blend_mode);
    result.metadata = BuildPreviewMetadata(tiles, options, 1);
    return result;
}

LiveStitchPreviewResult LiveStitchPreviewBuilder::Build(
    const std::vector<StitchTile>& tiles,
    LiveStitchPreviewOptions options)
{
    return Build(BuildTileRefs(tiles), options);
}
