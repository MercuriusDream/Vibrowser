#include <gtest/gtest.h>

#include <clever/layout/box.h>
#include <clever/layout/layout_engine.h>

#include <memory>
#include <string>

using namespace clever::layout;

namespace {

std::unique_ptr<LayoutNode> make_table() {
    auto node = std::make_unique<LayoutNode>();
    node->tag_name = "table";
    node->mode = LayoutMode::Table;
    node->display = DisplayType::Table;
    node->table_layout = 0;  // auto
    node->border_spacing = 0.0f;
    return node;
}

std::unique_ptr<LayoutNode> make_row() {
    auto node = std::make_unique<LayoutNode>();
    node->tag_name = "tr";
    node->mode = LayoutMode::Block;
    node->display = DisplayType::TableRow;
    return node;
}

std::unique_ptr<LayoutNode> make_cell() {
    auto node = std::make_unique<LayoutNode>();
    node->tag_name = "td";
    node->mode = LayoutMode::Block;
    node->display = DisplayType::TableCell;
    return node;
}

std::unique_ptr<LayoutNode> make_text(const std::string& text, float font_size = 16.0f) {
    auto node = std::make_unique<LayoutNode>();
    node->is_text = true;
    node->text_content = text;
    node->font_size = font_size;
    node->mode = LayoutMode::Inline;
    node->display = DisplayType::Inline;
    return node;
}

std::unique_ptr<LayoutNode> make_hn_title_subtext_table(float table_width) {
    auto table = make_table();
    table->specified_width = table_width;

    auto title_row = make_row();

    auto rank_cell = make_cell();
    rank_cell->specified_width = 28.0f;
    rank_cell->append_child(make_text("17.", 14.0f));

    auto vote_cell = make_cell();
    vote_cell->specified_width = 20.0f;
    vote_cell->append_child(make_text("^", 14.0f));

    auto title_cell = make_cell();
    title_cell->append_child(make_text(
        "Show HN: a narrow auto-width table title should wrap inside the remaining column instead of collapsing to its longest word",
        14.0f));

    title_row->append_child(std::move(rank_cell));
    title_row->append_child(std::move(vote_cell));
    title_row->append_child(std::move(title_cell));

    auto subtext_row = make_row();

    auto subtext_rank_cell = make_cell();
    subtext_rank_cell->specified_width = 28.0f;

    auto subtext_vote_cell = make_cell();
    subtext_vote_cell->specified_width = 20.0f;

    auto subtext_cell = make_cell();
    subtext_cell->append_child(make_text(
        "410 points by exampleuser 2 hours ago | hide | 89 comments",
        12.0f));

    subtext_row->append_child(std::move(subtext_rank_cell));
    subtext_row->append_child(std::move(subtext_vote_cell));
    subtext_row->append_child(std::move(subtext_cell));

    table->append_child(std::move(title_row));
    table->append_child(std::move(subtext_row));

    return table;
}

std::unique_ptr<LayoutNode> make_rowspan_colspan_table(float table_width) {
    auto table = make_table();
    table->specified_width = table_width;

    auto first_row = make_row();
    auto rowspan_cell = make_cell();
    rowspan_cell->specified_width = 48.0f;
    rowspan_cell->rowspan = 2;
    rowspan_cell->append_child(make_text("1.", 14.0f));

    auto wide_cell = make_cell();
    wide_cell->colspan = 2;
    wide_cell->append_child(make_text(
        "Wide title cell should span the remaining columns without disturbing rowspan occupancy",
        14.0f));

    first_row->append_child(std::move(rowspan_cell));
    first_row->append_child(std::move(wide_cell));

    auto second_row = make_row();
    auto meta_cell = make_cell();
    meta_cell->append_child(make_text("site.example", 12.0f));

    auto comments_cell = make_cell();
    comments_cell->append_child(make_text("42 comments", 12.0f));

    second_row->append_child(std::move(meta_cell));
    second_row->append_child(std::move(comments_cell));

    table->append_child(std::move(first_row));
    table->append_child(std::move(second_row));
    return table;
}

}  // namespace

TEST(TableLayoutRegression, HnLikeAutoLayoutKeepsSpacerRowHeightAndWideTitleColumn) {
    auto table = make_table();
    table->specified_width = 600.0f;

    // HN-like data row: rank, vote, title.
    auto data_row = make_row();

    auto rank_cell = make_cell();
    rank_cell->specified_width = 28.0f;
    rank_cell->append_child(make_text("1.", 14.0f));

    auto vote_cell = make_cell();
    vote_cell->specified_width = 20.0f;
    vote_cell->append_child(make_text("^", 14.0f));

    auto title_cell = make_cell();
    title_cell->append_child(make_text("Show HN: a deliberately long title to verify the third column receives shortfall width", 14.0f));

    data_row->append_child(std::move(rank_cell));
    data_row->append_child(std::move(vote_cell));
    data_row->append_child(std::move(title_cell));

    // Spacer row used by HN markup between title and metadata rows.
    auto spacer_row = make_row();
    spacer_row->specified_height = 5.0f;
    auto spacer_cell = make_cell();
    spacer_cell->colspan = 3;
    spacer_row->append_child(std::move(spacer_cell));

    table->append_child(std::move(data_row));
    table->append_child(std::move(spacer_row));

    LayoutEngine engine;
    engine.compute(*table, 600.0f, 600.0f);

    ASSERT_EQ(table->children.size(), 2u);
    auto* laid_data_row = table->children[0].get();
    auto* laid_spacer_row = table->children[1].get();
    ASSERT_EQ(laid_data_row->children.size(), 3u);

    const float rank_w = laid_data_row->children[0]->geometry.width;
    const float vote_w = laid_data_row->children[1]->geometry.width;
    const float title_w = laid_data_row->children[2]->geometry.width;

    EXPECT_GT(title_w, rank_w);
    EXPECT_GT(title_w, vote_w);
    EXPECT_GT(title_w, 450.0f)
        << "Title column should consume most of the table width in HN-like rows";

    EXPECT_GE(laid_spacer_row->geometry.height, 5.0f)
        << "Explicit spacer row height must be preserved";
    EXPECT_GE(laid_spacer_row->geometry.y, laid_data_row->geometry.height)
        << "Spacer row should be placed below the data row";
}

TEST(TableLayoutRegression, AutoLayoutScalesAllExplicitColumnsProportionallyOnShortfall) {
    auto table = make_table();
    table->specified_width = 600.0f;

    auto row = make_row();
    auto c1 = make_cell();
    c1->specified_width = 100.0f;
    c1->append_child(make_text("A"));
    auto c2 = make_cell();
    c2->specified_width = 200.0f;
    c2->append_child(make_text("B"));
    auto c3 = make_cell();
    c3->specified_width = 50.0f;
    c3->append_child(make_text("C"));

    row->append_child(std::move(c1));
    row->append_child(std::move(c2));
    row->append_child(std::move(c3));
    table->append_child(std::move(row));

    LayoutEngine engine;
    engine.compute(*table, 600.0f, 400.0f);

    ASSERT_EQ(table->children.size(), 1u);
    ASSERT_EQ(table->children[0]->children.size(), 3u);

    const float w1 = table->children[0]->children[0]->geometry.width;
    const float w2 = table->children[0]->children[1]->geometry.width;
    const float w3 = table->children[0]->children[2]->geometry.width;

    // The original explicit widths are 100:200:50 (ratio 2:4:1). After filling
    // shortfall to table width, ratios should remain approximately proportional.
    ASSERT_GT(w1, 0.0f);
    ASSERT_GT(w2, 0.0f);
    ASSERT_GT(w3, 0.0f);
    EXPECT_NEAR(w2 / w1, 2.0f, 0.1f);
    EXPECT_NEAR(w3 / w1, 0.5f, 0.06f);
}

TEST(TableLayoutRegression, AutoLayoutKeepsColspanAndRowspanPlacementStable) {
    auto table = make_rowspan_colspan_table(360.0f);

    LayoutEngine engine;
    engine.compute(*table, 360.0f, 240.0f);

    ASSERT_EQ(table->children.size(), 2u);
    auto* laid_first_row = table->children[0].get();
    auto* laid_second_row = table->children[1].get();
    ASSERT_EQ(laid_first_row->children.size(), 2u);
    ASSERT_EQ(laid_second_row->children.size(), 2u);

    auto* laid_rowspan_cell = laid_first_row->children[0].get();
    auto* laid_wide_cell = laid_first_row->children[1].get();
    auto* laid_meta_cell = laid_second_row->children[0].get();
    auto* laid_comments_cell = laid_second_row->children[1].get();

    EXPECT_GT(laid_rowspan_cell->geometry.width, 40.0f);
    EXPECT_GT(laid_wide_cell->geometry.width, laid_comments_cell->geometry.width);
    EXPECT_NEAR(laid_wide_cell->geometry.width,
                laid_meta_cell->geometry.width + laid_comments_cell->geometry.width,
                0.1f);
    EXPECT_NEAR(laid_meta_cell->geometry.x, laid_rowspan_cell->geometry.width, 0.1f);
    EXPECT_NEAR(laid_comments_cell->geometry.x,
                laid_meta_cell->geometry.x + laid_meta_cell->geometry.width,
                0.1f);
    EXPECT_GE(laid_rowspan_cell->geometry.height,
              laid_first_row->geometry.height + laid_second_row->geometry.height);
}

TEST(LayoutTableRegressionTest, HackerNewsTitleSubtextWrapUsesAvailableColumnWidthV2065) {
    auto table = make_hn_title_subtext_table(260.0f);

    LayoutEngine engine;
    engine.compute(*table, 260.0f, 240.0f);

    ASSERT_EQ(table->children.size(), 2u);
    ASSERT_EQ(table->children[0]->children.size(), 3u);
    ASSERT_EQ(table->children[1]->children.size(), 3u);

    auto* title_row = table->children[0].get();
    auto* subtext_row = table->children[1].get();

    const float rank_width = title_row->children[0]->geometry.width;
    const float vote_width = title_row->children[1]->geometry.width;
    const float title_width = title_row->children[2]->geometry.width;
    const float subtext_width = subtext_row->children[2]->geometry.width;
    const float expected_auto_column_width = table->geometry.width - rank_width - vote_width;

    EXPECT_NEAR(title_width, expected_auto_column_width, 0.5f);
    EXPECT_NEAR(subtext_width, expected_auto_column_width, 0.5f);
    EXPECT_GT(title_width, 200.0f)
        << "Auto title/subtext column should keep most of the remaining table width";
    EXPECT_GT(title_row->geometry.height, 30.0f)
        << "The title row should wrap vertically in the narrow column rather than forcing min-content width";
}

TEST(LayoutTableRegressionTest, AutoWidthColumnDoesNotCollapseToMinContentAfterRelayoutV2065) {
    auto table = make_hn_title_subtext_table(520.0f);

    LayoutEngine engine;
    engine.set_text_measurer([](const std::string& measured, float font_size, const std::string&,
                                int, bool, float) {
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });
    engine.compute(*table, 520.0f, 240.0f);

    ASSERT_EQ(table->children.size(), 2u);
    const float wide_title_width = table->children[0]->children[2]->geometry.width;
    const float wide_subtext_width = table->children[1]->children[2]->geometry.width;

    table->specified_width = 260.0f;
    engine.compute(*table, 260.0f, 240.0f);

    ASSERT_EQ(table->children[0]->children.size(), 3u);
    ASSERT_EQ(table->children[1]->children.size(), 3u);

    auto* narrow_title_row = table->children[0].get();
    auto* narrow_subtext_row = table->children[1].get();
    const float narrow_rank_width = narrow_title_row->children[0]->geometry.width;
    const float narrow_vote_width = narrow_title_row->children[1]->geometry.width;
    const float narrow_title_width = narrow_title_row->children[2]->geometry.width;
    const float narrow_subtext_width = narrow_subtext_row->children[2]->geometry.width;
    const float expected_narrow_auto_column_width =
        table->geometry.width - narrow_rank_width - narrow_vote_width;

    EXPECT_LT(narrow_title_width, wide_title_width);
    EXPECT_LT(narrow_subtext_width, wide_subtext_width);
    EXPECT_NEAR(narrow_title_width, expected_narrow_auto_column_width, 0.5f);
    EXPECT_NEAR(narrow_subtext_width, expected_narrow_auto_column_width, 0.5f);
    EXPECT_GT(narrow_title_width, 200.0f)
        << "Relayout should keep the auto column at remaining width instead of collapsing to min-content";
    EXPECT_GT(narrow_title_row->geometry.height, 30.0f);
}

TEST(LayoutTableRegressionTest, AutoWidthColumnReexpandsFromFreshHintsAfterShrinkV2071) {
    auto table = make_hn_title_subtext_table(520.0f);

    LayoutEngine engine;
    engine.compute(*table, 520.0f, 240.0f);

    const float wide_title_width = table->children[0]->children[2]->geometry.width;
    const float wide_title_height = table->children[0]->geometry.height;

    table->specified_width = 260.0f;
    engine.compute(*table, 260.0f, 240.0f);

    const float narrow_title_width = table->children[0]->children[2]->geometry.width;
    const float narrow_title_height = table->children[0]->geometry.height;

    table->specified_width = 420.0f;
    engine.compute(*table, 420.0f, 240.0f);

    ASSERT_EQ(table->children.size(), 2u);
    auto* expanded_title_row = table->children[0].get();
    auto* expanded_subtext_row = table->children[1].get();

    const float rank_width = expanded_title_row->children[0]->geometry.width;
    const float vote_width = expanded_title_row->children[1]->geometry.width;
    const float expanded_title_width = expanded_title_row->children[2]->geometry.width;
    const float expanded_subtext_width = expanded_subtext_row->children[2]->geometry.width;
    const float expanded_title_height = expanded_title_row->geometry.height;
    const float expected_expanded_auto_column_width = table->geometry.width - rank_width - vote_width;

    EXPECT_LT(narrow_title_width, wide_title_width);
    EXPECT_GT(expanded_title_width, narrow_title_width);
    EXPECT_NEAR(expanded_title_width, expected_expanded_auto_column_width, 0.5f);
    EXPECT_NEAR(expanded_subtext_width, expected_expanded_auto_column_width, 0.5f);
    EXPECT_LT(expanded_title_height, narrow_title_height)
        << "Widening the table should recompute the title column from fresh hints and reduce wrapping";
    EXPECT_LE(expanded_title_height, wide_title_height + 0.5f);
}

TEST(LayoutTableRegressionTest, AutoWidthColumnReusesFreshHintsAfterWidthOscillationV2083) {
    auto table = make_hn_title_subtext_table(520.0f);

    LayoutEngine engine;

    table->specified_width = 520.0f;
    engine.compute(*table, 520.0f, 240.0f);
    const float initial_wide_title =
        table->children[0]->children[2]->geometry.width;
    const float initial_subtext_title =
        table->children[1]->children[2]->geometry.width;

    table->specified_width = 260.0f;
    engine.compute(*table, 260.0f, 240.0f);
    const float narrow_title_once =
        table->children[0]->children[2]->geometry.width;
    const float narrow_subtext_once =
        table->children[1]->children[2]->geometry.width;

    table->specified_width = 520.0f;
    engine.compute(*table, 520.0f, 240.0f);
    const float rewide_title_once =
        table->children[0]->children[2]->geometry.width;
    const float rewide_subtext_once =
        table->children[1]->children[2]->geometry.width;

    table->specified_width = 260.0f;
    engine.compute(*table, 260.0f, 240.0f);
    const float narrow_title_twice =
        table->children[0]->children[2]->geometry.width;
    const float narrow_subtext_twice =
        table->children[1]->children[2]->geometry.width;

    table->specified_width = 520.0f;
    engine.compute(*table, 520.0f, 240.0f);
    const float rewide_title_twice =
        table->children[0]->children[2]->geometry.width;
    const float rewide_subtext_twice =
        table->children[1]->children[2]->geometry.width;

    const float rank_width = table->children[0]->children[0]->geometry.width;
    const float vote_width = table->children[0]->children[1]->geometry.width;
    const float expected_wide_auto_width = table->geometry.width - rank_width - vote_width;

    EXPECT_GT(initial_wide_title, 200.0f);
    EXPECT_GT(rewide_title_once, narrow_title_once);
    EXPECT_GT(rewide_title_twice, narrow_title_twice);
    EXPECT_NEAR(rewide_title_once, rewide_title_twice, 0.5f)
        << "Re-expanding twice should reuse intrinsic hints and keep wide auto columns stable";
    EXPECT_NEAR(rewide_subtext_once, rewide_subtext_twice, 0.5f);
    EXPECT_NEAR(rewide_title_twice, expected_wide_auto_width, 0.5f);
    EXPECT_NEAR(rewide_subtext_twice, expected_wide_auto_width, 0.5f);
    EXPECT_LT(narrow_title_once, initial_wide_title);
    EXPECT_LT(narrow_title_twice, rewide_title_twice);
    EXPECT_NEAR(narrow_title_once, narrow_title_twice, 0.5f);
    EXPECT_NEAR(initial_wide_title, expected_wide_auto_width, 0.5f);
    EXPECT_NEAR(initial_subtext_title, initial_wide_title, 0.5f);
    EXPECT_NEAR(narrow_subtext_once, narrow_title_once, 0.5f);
    EXPECT_NEAR(narrow_subtext_twice, narrow_title_twice, 0.5f);
}

TEST(LayoutTableRegressionTest, TableRelayoutRowRemovalKeepsOutsideSiblingMemoHotV2098) {
    auto table = make_table();
    table->specified_width = -3.0f;

    auto header_row = make_row();
    auto outside_left = make_cell();
    outside_left->append_child(make_text("steady signal", 16.0f));
    auto shared_left = make_cell();
    shared_left->append_child(make_text("shared alpha", 16.0f));
    auto shared_right = make_cell();
    shared_right->append_child(make_text("shared beta", 16.0f));
    header_row->append_child(std::move(outside_left));
    header_row->append_child(std::move(shared_left));
    header_row->append_child(std::move(shared_right));

    auto removable_row = make_row();
    auto row_label = make_cell();
    row_label->specified_width = 36.0f;
    row_label->append_child(make_text("1.", 14.0f));
    auto spanning_cell = make_cell();
    spanning_cell->colspan = 2;
    spanning_cell->white_space = 1;
    spanning_cell->append_child(make_text("ultrawideheadlinevalue", 16.0f));
    removable_row->append_child(std::move(row_label));
    removable_row->append_child(std::move(spanning_cell));

    auto footer_row = make_row();
    auto outside_footer = make_cell();
    outside_footer->append_child(make_text("calm marker", 16.0f));
    auto active_left = make_cell();
    active_left->append_child(make_text("active lane", 16.0f));
    auto active_right = make_cell();
    active_right->append_child(make_text("active notes", 16.0f));
    footer_row->append_child(std::move(outside_footer));
    footer_row->append_child(std::move(active_left));
    footer_row->append_child(std::move(active_right));

    table->append_child(std::move(header_row));
    table->append_child(std::move(removable_row));
    table->append_child(std::move(footer_row));

    int steady_measurements = 0;
    int signal_measurements = 0;
    int calm_measurements = 0;
    int marker_measurements = 0;
    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "steady") {
            steady_measurements++;
        } else if (measured == "signal") {
            signal_measurements++;
        } else if (measured == "calm") {
            calm_measurements++;
        } else if (measured == "marker") {
            marker_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 420.0f, 240.0f);

    table->children.erase(table->children.begin() + 1);
    engine.compute(*table, 420.0f, 240.0f);

    ASSERT_EQ(table->children.size(), 2u);
    EXPECT_EQ(steady_measurements, 1);
    EXPECT_EQ(signal_measurements, 1);
    EXPECT_EQ(calm_measurements, 1);
    EXPECT_EQ(marker_measurements, 1)
        << "cells outside the removed colspan path should keep their memoized intrinsic widths";
}

TEST(LayoutTableRegressionTest, TableRelayoutColspanShrinkKeepsOutsideSiblingMemoHotV2099) {
    auto table = make_table();
    table->specified_width = -3.0f;

    auto header_row = make_row();
    auto outside_left = make_cell();
    outside_left->append_child(make_text("steady signal", 16.0f));
    auto shared_left = make_cell();
    shared_left->append_child(make_text("shared alpha", 16.0f));
    auto shared_right = make_cell();
    shared_right->append_child(make_text("shared beta", 16.0f));
    header_row->append_child(std::move(outside_left));
    header_row->append_child(std::move(shared_left));
    header_row->append_child(std::move(shared_right));

    auto shrinking_row = make_row();
    auto row_label = make_cell();
    row_label->specified_width = 36.0f;
    row_label->append_child(make_text("1.", 14.0f));
    auto spanning_cell = make_cell();
    LayoutNode* spanning_cell_ptr = spanning_cell.get();
    spanning_cell->colspan = 2;
    spanning_cell->append_child(make_text("stretching delta", 16.0f));
    LayoutNode* shrinking_row_ptr = shrinking_row.get();
    shrinking_row->append_child(std::move(row_label));
    shrinking_row->append_child(std::move(spanning_cell));

    auto footer_row = make_row();
    auto outside_footer = make_cell();
    outside_footer->append_child(make_text("calm marker", 16.0f));
    auto active_left = make_cell();
    active_left->append_child(make_text("active lane", 16.0f));
    auto active_right = make_cell();
    active_right->append_child(make_text("active notes", 16.0f));
    footer_row->append_child(std::move(outside_footer));
    footer_row->append_child(std::move(active_left));
    footer_row->append_child(std::move(active_right));

    table->append_child(std::move(header_row));
    table->append_child(std::move(shrinking_row));
    table->append_child(std::move(footer_row));

    int steady_measurements = 0;
    int signal_measurements = 0;
    int calm_measurements = 0;
    int marker_measurements = 0;
    int tiny_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "steady") {
            steady_measurements++;
        } else if (measured == "signal") {
            signal_measurements++;
        } else if (measured == "calm") {
            calm_measurements++;
        } else if (measured == "marker") {
            marker_measurements++;
        } else if (measured == "tiny") {
            tiny_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 420.0f, 240.0f);
    const float wide_spanning_width = spanning_cell_ptr->geometry.width;

    spanning_cell_ptr->colspan = 1;
    auto new_cell = make_cell();
    new_cell->append_child(make_text("tiny delta", 16.0f));
    shrinking_row_ptr->append_child(std::move(new_cell));
    engine.compute(*table, 420.0f, 240.0f);
    const float narrow_spanning_width = spanning_cell_ptr->geometry.width;

    ASSERT_EQ(table->children.size(), 3u);
    ASSERT_EQ(table->children[1]->children.size(), 3u);
    EXPECT_LT(narrow_spanning_width, wide_spanning_width)
        << "shrinking colspan should evict the stale span-wide intrinsic hint for the affected cell";
    EXPECT_EQ(steady_measurements, 1);
    EXPECT_EQ(signal_measurements, 1);
    EXPECT_EQ(calm_measurements, 1);
    EXPECT_EQ(marker_measurements, 1)
        << "cells outside the shrinking colspan path should keep their memoized intrinsic widths";
    EXPECT_EQ(tiny_measurements, 1);
}

TEST(LayoutTableRegressionTest, TableCellShrinkWrapRelayoutReusesMinContentMemoPathV2088) {
    auto table = make_table();
    table->specified_width = 520.0f;

    auto row = make_row();

    auto rank_cell = make_cell();
    rank_cell->specified_width = 28.0f;
    rank_cell->append_child(make_text("1.", 14.0f));

    auto title_cell = make_cell();
    auto inline_block = std::make_unique<LayoutNode>();
    inline_block->tag_name = "span";
    inline_block->mode = LayoutMode::InlineBlock;
    inline_block->display = DisplayType::InlineBlock;
    inline_block->append_child(make_text("alpha beta gamma", 16.0f));
    title_cell->append_child(std::move(inline_block));

    row->append_child(std::move(rank_cell));
    row->append_child(std::move(title_cell));
    table->append_child(std::move(row));

    int full_text_measurements = 0;
    int word_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "alpha beta gamma") {
            full_text_measurements++;
        } else if (measured == "alpha" || measured == "beta" || measured == "gamma") {
            word_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 520.0f, 240.0f);
    table->specified_width = 420.0f;
    engine.compute(*table, 420.0f, 240.0f);
    table->specified_width = 520.0f;
    engine.compute(*table, 520.0f, 240.0f);

    ASSERT_EQ(table->children.size(), 1u);
    ASSERT_EQ(table->children[0]->children.size(), 2u);
    EXPECT_EQ(full_text_measurements, 4)
        << "table relayout should only pay one max-content measurement for the inline-block subtree beyond the repeated layout passes";
    EXPECT_EQ(word_measurements, 3)
        << "table relayout should reuse the inline-block subtree's min-content hint after the first pass";
    EXPECT_GT(engine.relayout_max_content_width_memo_hit_count(), 0)
        << "unchanged table inline-block subtrees should hit the relayout max-content memo path";
    EXPECT_GT(engine.relayout_min_content_width_memo_hit_count(), 0)
        << "unchanged table inline-block subtrees should hit the relayout min-content memo path";
    EXPECT_GT(table->children[0]->children[1]->geometry.width, 0.0f);
}

TEST(LayoutTableRegressionTest, AutoLayoutInvalidatesMutatedCellHintV2089) {
    auto table = make_table();
    table->specified_width = 320.0f;

    auto row = make_row();
    auto first_cell = make_cell();
    auto mutating_text = make_text("stretching delta", 16.0f);
    LayoutNode* mutating_text_ptr = mutating_text.get();
    first_cell->append_child(std::move(mutating_text));

    auto second_cell = make_cell();
    second_cell->append_child(make_text("steady signal", 16.0f));

    row->append_child(std::move(first_cell));
    row->append_child(std::move(second_cell));
    table->append_child(std::move(row));

    int steady_measurements = 0;
    int signal_measurements = 0;
    int stretching_measurements = 0;
    int tiny_measurements = 0;
    int delta_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "steady") {
            steady_measurements++;
        } else if (measured == "signal") {
            signal_measurements++;
        } else if (measured == "stretching") {
            stretching_measurements++;
        } else if (measured == "tiny") {
            tiny_measurements++;
        } else if (measured == "delta") {
            delta_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 320.0f, 240.0f);
    const float wide_first_col = table->children[0]->children[0]->geometry.width;
    const float narrow_second_col = table->children[0]->children[1]->geometry.width;

    mutating_text_ptr->text_content = "tiny delta";
    engine.compute(*table, 320.0f, 240.0f);
    const float narrow_first_col = table->children[0]->children[0]->geometry.width;
    const float wide_second_col = table->children[0]->children[1]->geometry.width;

    EXPECT_LT(narrow_first_col, wide_first_col);
    EXPECT_GT(wide_second_col, narrow_second_col);
    EXPECT_LT(narrow_first_col, wide_second_col);
    EXPECT_EQ(steady_measurements, 1)
        << "unchanged sibling cells should keep their cached intrinsic width hint";
    EXPECT_EQ(signal_measurements, 1);
    EXPECT_EQ(stretching_measurements, 1);
    EXPECT_EQ(tiny_measurements, 1)
        << "the mutated cell should be remeasured once its intrinsic width changes";
    EXPECT_EQ(delta_measurements, 2);
}

TEST(LayoutTableRegressionTest, AutoLayoutInvalidatesRetainedHintOnWhiteSpaceNowrapMutationV2100) {
    auto table = make_table();
    table->specified_width = 320.0f;

    auto row = make_row();
    auto mutating_cell = make_cell();
    LayoutNode* mutating_cell_ptr = mutating_cell.get();
    mutating_cell->append_child(make_text("alpha beta gamma", 16.0f));

    auto steady_cell = make_cell();
    steady_cell->append_child(make_text("steady signal", 16.0f));

    row->append_child(std::move(mutating_cell));
    row->append_child(std::move(steady_cell));
    table->append_child(std::move(row));

    int full_text_measurements = 0;
    int alpha_measurements = 0;
    int beta_measurements = 0;
    int gamma_measurements = 0;
    int steady_measurements = 0;
    int signal_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "alpha beta gamma") {
            full_text_measurements++;
        } else if (measured == "alpha") {
            alpha_measurements++;
        } else if (measured == "beta") {
            beta_measurements++;
        } else if (measured == "gamma") {
            gamma_measurements++;
        } else if (measured == "steady") {
            steady_measurements++;
        } else if (measured == "signal") {
            signal_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 320.0f, 240.0f);
    const float wrapping_first_col = table->children[0]->children[0]->geometry.width;
    const float wrapping_second_col = table->children[0]->children[1]->geometry.width;
    const int full_text_measurements_after_wrapping = full_text_measurements;
    const int alpha_measurements_after_wrapping = alpha_measurements;
    const int beta_measurements_after_wrapping = beta_measurements;
    const int gamma_measurements_after_wrapping = gamma_measurements;

    mutating_cell_ptr->white_space_nowrap = true;
    engine.compute(*table, 320.0f, 240.0f);
    const float nowrap_first_col = table->children[0]->children[0]->geometry.width;
    const float nowrap_second_col = table->children[0]->children[1]->geometry.width;
    const int full_text_measurements_after_nowrap = full_text_measurements;

    EXPECT_GT(nowrap_first_col, wrapping_first_col)
        << "switching a cell to nowrap should invalidate the retained min-content hint";
    EXPECT_LT(nowrap_second_col, wrapping_second_col);
    EXPECT_GT(full_text_measurements_after_nowrap, full_text_measurements_after_wrapping)
        << "the nowrap mutation should force the mutated cell onto the max-content hint path";
    EXPECT_EQ(alpha_measurements, alpha_measurements_after_wrapping);
    EXPECT_EQ(beta_measurements, beta_measurements_after_wrapping);
    EXPECT_EQ(gamma_measurements, gamma_measurements_after_wrapping);
    EXPECT_EQ(steady_measurements, 1);
    EXPECT_EQ(signal_measurements, 1)
        << "unchanged siblings should keep their cached intrinsic width hints";
}

TEST(LayoutTableRegressionTest, AutoLayoutEvictsRetainedHintAcrossWhiteSpaceNowrapFlipFlopV2101) {
    auto table = make_table();
    table->specified_width = 320.0f;

    auto row = make_row();
    auto mutating_cell = make_cell();
    LayoutNode* mutating_cell_ptr = mutating_cell.get();
    mutating_cell->append_child(make_text("alpha beta gamma", 16.0f));

    auto steady_cell = make_cell();
    steady_cell->append_child(make_text("steady signal", 16.0f));

    row->append_child(std::move(mutating_cell));
    row->append_child(std::move(steady_cell));
    table->append_child(std::move(row));

    int full_text_measurements = 0;
    int alpha_measurements = 0;
    int beta_measurements = 0;
    int gamma_measurements = 0;
    int steady_measurements = 0;
    int signal_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "alpha beta gamma") {
            full_text_measurements++;
        } else if (measured == "alpha") {
            alpha_measurements++;
        } else if (measured == "beta") {
            beta_measurements++;
        } else if (measured == "gamma") {
            gamma_measurements++;
        } else if (measured == "steady") {
            steady_measurements++;
        } else if (measured == "signal") {
            signal_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 320.0f, 240.0f);
    const float wrapping_first_col = table->children[0]->children[0]->geometry.width;
    mutating_cell_ptr->white_space_nowrap = true;
    engine.compute(*table, 320.0f, 240.0f);
    const float nowrap_first_col = table->children[0]->children[0]->geometry.width;
    const int full_text_measurements_after_nowrap = full_text_measurements;

    mutating_cell_ptr->white_space_nowrap = false;
    engine.compute(*table, 320.0f, 240.0f);
    const float rewrapped_first_col = table->children[0]->children[0]->geometry.width;

    EXPECT_GT(nowrap_first_col, wrapping_first_col);
    EXPECT_LT(rewrapped_first_col, nowrap_first_col);
    EXPECT_NEAR(rewrapped_first_col, wrapping_first_col, 1.0f);
    EXPECT_GT(full_text_measurements_after_nowrap, 0);
    EXPECT_EQ(steady_measurements, 1);
    EXPECT_EQ(signal_measurements, 1)
        << "stable siblings should keep their cached intrinsic width hints across nowrap flip-flops";
}

TEST(LayoutTableRegressionTest, AutoLayoutInvalidatesRetainedHintOnMinWidthMutationV2101) {
    auto table = make_table();
    table->specified_width = -3.0f;

    auto row = make_row();
    auto mutating_cell = make_cell();
    LayoutNode* mutating_cell_ptr = mutating_cell.get();
    mutating_cell->append_child(make_text("tiny delta", 16.0f));

    auto steady_cell = make_cell();
    steady_cell->append_child(make_text("steady signal", 16.0f));

    row->append_child(std::move(mutating_cell));
    row->append_child(std::move(steady_cell));
    table->append_child(std::move(row));

    int tiny_measurements = 0;
    int delta_measurements = 0;
    int steady_measurements = 0;
    int signal_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "tiny") {
            tiny_measurements++;
        } else if (measured == "delta") {
            delta_measurements++;
        } else if (measured == "steady") {
            steady_measurements++;
        } else if (measured == "signal") {
            signal_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 420.0f, 240.0f);
    const float initial_table_width = table->geometry.width;
    const float initial_first_col = table->children[0]->children[0]->geometry.width;

    mutating_cell_ptr->min_width = initial_first_col + 80.0f;
    table->specified_width = -3.0f;
    engine.compute(*table, 420.0f, 240.0f);
    const float widened_table_width = table->geometry.width;
    const float widened_first_col = table->children[0]->children[0]->geometry.width;

    EXPECT_GT(widened_table_width, initial_table_width);
    EXPECT_GT(widened_first_col, initial_first_col + 40.0f)
        << "changing a cell min-width should invalidate the retained intrinsic hint for that cell";
    EXPECT_EQ(steady_measurements, 1);
    EXPECT_EQ(signal_measurements, 1)
        << "stable siblings should keep their cached intrinsic width hints across width-input mutations";
    EXPECT_EQ(tiny_measurements, 1);
    EXPECT_EQ(delta_measurements, 1);
}

TEST(LayoutTableRegressionTest, AutoLayoutInvalidatesRetainedHintOnTextMeasurerChangeV2101) {
    auto table = make_table();
    table->specified_width = -3.0f;

    auto row = make_row();
    auto first_cell = make_cell();
    first_cell->append_child(make_text("alpha beta gamma", 16.0f));
    auto second_cell = make_cell();
    second_cell->append_child(make_text("steady signal", 16.0f));
    row->append_child(std::move(first_cell));
    row->append_child(std::move(second_cell));
    table->append_child(std::move(row));

    LayoutEngine engine;
    engine.set_text_measurer([](const std::string& measured, float font_size, const std::string&,
                                int, bool, float) {
        return static_cast<float>(measured.size()) * font_size * 0.45f;
    });

    engine.compute(*table, 420.0f, 240.0f);
    const float narrow_table_width = table->geometry.width;
    const float narrow_first_col = table->children[0]->children[0]->geometry.width;

    engine.set_text_measurer([](const std::string& measured, float font_size, const std::string&,
                                int, bool, float) {
        return static_cast<float>(measured.size()) * font_size * 0.9f;
    });
    table->specified_width = -3.0f;
    engine.compute(*table, 420.0f, 240.0f);
    const float wide_table_width = table->geometry.width;
    const float wide_first_col = table->children[0]->children[0]->geometry.width;

    EXPECT_GT(wide_table_width, narrow_table_width);
    EXPECT_GT(wide_first_col, narrow_first_col)
        << "changing the text measurer on a hot layout engine should invalidate retained intrinsic width hints";
}

TEST(LayoutTableRegressionTest, TableRelayoutMaxContentMemoInvalidatesOnInlineBlockDescendantMutationV2090) {
    auto table = make_table();
    table->specified_width = 520.0f;

    auto row = make_row();

    auto label_cell = make_cell();
    label_cell->specified_width = 36.0f;
    label_cell->append_child(make_text("1.", 14.0f));

    auto title_cell = make_cell();
    auto inline_block = std::make_unique<LayoutNode>();
    inline_block->tag_name = "span";
    inline_block->mode = LayoutMode::InlineBlock;
    inline_block->display = DisplayType::InlineBlock;
    auto mutating_text = make_text("alpha beta gamma", 16.0f);
    LayoutNode* mutating_text_ptr = mutating_text.get();
    inline_block->append_child(std::move(mutating_text));
    title_cell->append_child(std::move(inline_block));

    row->append_child(std::move(label_cell));
    row->append_child(std::move(title_cell));
    table->append_child(std::move(row));

    LayoutEngine engine;
    engine.set_text_measurer([](const std::string& measured, float font_size, const std::string&,
                                int, bool, float) {
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });
    engine.compute(*table, 520.0f, 240.0f);
    const int hits_after_first_compute = engine.relayout_max_content_width_memo_hit_count();

    table->specified_width = 420.0f;
    engine.compute(*table, 420.0f, 240.0f);
    const int hits_after_first_relayout = engine.relayout_max_content_width_memo_hit_count();
    EXPECT_GT(hits_after_first_relayout, hits_after_first_compute);

    table->specified_width = 520.0f;
    engine.compute(*table, 520.0f, 240.0f);
    const int hits_after_unchanged_relayout = engine.relayout_max_content_width_memo_hit_count();
    EXPECT_GT(hits_after_unchanged_relayout, hits_after_first_relayout);

    mutating_text_ptr->text_content = "tiny delta";
    table->specified_width = 420.0f;
    engine.compute(*table, 420.0f, 240.0f);
    EXPECT_EQ(engine.relayout_max_content_width_memo_hit_count(), hits_after_unchanged_relayout)
        << "changing an inline-block descendant should invalidate the cached max-content relayout hint";

    engine.compute(*table, 420.0f, 240.0f);
    EXPECT_GT(engine.relayout_max_content_width_memo_hit_count(), hits_after_unchanged_relayout)
        << "once the mutated subtree stabilizes again, repeated relayout should memo-hit again";
}

TEST(LayoutTableRegressionTest, TableRelayoutMaxContentKeepsUnchangedNestedSiblingsMemoHotV2093) {
    auto table = make_table();
    table->specified_width = 520.0f;

    auto row = make_row();
    auto cell = make_cell();
    cell->white_space = 1;  // nowrap => table auto layout uses max-content width hints.

    auto stable_inline_block = std::make_unique<LayoutNode>();
    stable_inline_block->tag_name = "span";
    stable_inline_block->mode = LayoutMode::InlineBlock;
    stable_inline_block->display = DisplayType::InlineBlock;
    stable_inline_block->append_child(make_text("steady cluster", 16.0f));

    auto mutating_inline_block = std::make_unique<LayoutNode>();
    mutating_inline_block->tag_name = "span";
    mutating_inline_block->mode = LayoutMode::InlineBlock;
    mutating_inline_block->display = DisplayType::InlineBlock;
    LayoutNode* mutating_inline_block_ptr = mutating_inline_block.get();
    auto mutating_text = make_text("stretching delta", 16.0f);
    LayoutNode* mutating_text_ptr = mutating_text.get();
    mutating_inline_block->append_child(std::move(mutating_text));

    cell->append_child(std::move(stable_inline_block));
    cell->append_child(std::move(mutating_inline_block));
    row->append_child(std::move(cell));
    table->append_child(std::move(row));

    LayoutEngine engine;
    engine.set_text_measurer([](const std::string& measured, float font_size, const std::string&,
                                int, bool, float) {
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 520.0f, 240.0f);
    const int hits_after_first_compute = engine.relayout_max_content_width_memo_hit_count();
    const float wide_mutating_width = mutating_inline_block_ptr->geometry.width;

    mutating_text_ptr->text_content = "tiny delta";
    engine.compute(*table, 520.0f, 240.0f);
    const int hits_after_mutation = engine.relayout_max_content_width_memo_hit_count();
    const float narrow_mutating_width = mutating_inline_block_ptr->geometry.width;

    engine.compute(*table, 520.0f, 240.0f);
    EXPECT_LT(narrow_mutating_width, wide_mutating_width)
        << "changing one nested inline-block should still force the affected cell subtree to relayout";
    EXPECT_GT(hits_after_mutation, hits_after_first_compute)
        << "recomputing the mutated cell should still reuse relayout max-content hints for unchanged siblings";
    EXPECT_GT(engine.relayout_max_content_width_memo_hit_count(), hits_after_mutation)
        << "once the mutated subtree stabilizes again, repeated relayout should keep hitting the sibling memo path";
}

TEST(LayoutTableRegressionTest, TableRelayoutMinContentKeepsUnchangedNestedSiblingsMemoHotV2094) {
    auto table = make_table();
    table->specified_width = 320.0f;

    auto row = make_row();
    auto cell = make_cell();

    auto stable_inline_block = std::make_unique<LayoutNode>();
    stable_inline_block->tag_name = "span";
    stable_inline_block->mode = LayoutMode::InlineBlock;
    stable_inline_block->display = DisplayType::InlineBlock;
    stable_inline_block->append_child(make_text("steady signal", 16.0f));

    auto mutating_inline_block = std::make_unique<LayoutNode>();
    mutating_inline_block->tag_name = "span";
    mutating_inline_block->mode = LayoutMode::InlineBlock;
    mutating_inline_block->display = DisplayType::InlineBlock;
    LayoutNode* mutating_inline_block_ptr = mutating_inline_block.get();
    auto mutating_text = make_text("stretching delta", 16.0f);
    LayoutNode* mutating_text_ptr = mutating_text.get();
    mutating_inline_block->append_child(std::move(mutating_text));

    cell->append_child(std::move(stable_inline_block));
    cell->append_child(std::move(mutating_inline_block));
    row->append_child(std::move(cell));
    table->append_child(std::move(row));

    int steady_measurements = 0;
    int signal_measurements = 0;
    int stretching_measurements = 0;
    int tiny_measurements = 0;
    int delta_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "steady") {
            steady_measurements++;
        } else if (measured == "signal") {
            signal_measurements++;
        } else if (measured == "stretching") {
            stretching_measurements++;
        } else if (measured == "tiny") {
            tiny_measurements++;
        } else if (measured == "delta") {
            delta_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 320.0f, 240.0f);
    const float wide_mutating_width = mutating_inline_block_ptr->geometry.width;

    mutating_text_ptr->text_content = "tiny delta";
    engine.compute(*table, 320.0f, 240.0f);
    const float narrow_mutating_width = mutating_inline_block_ptr->geometry.width;

    EXPECT_LT(narrow_mutating_width, wide_mutating_width);
    EXPECT_EQ(steady_measurements, 1)
        << "unchanged nested siblings should keep their min-content memoized measurement";
    EXPECT_EQ(signal_measurements, 1);
    EXPECT_EQ(stretching_measurements, 1);
    EXPECT_EQ(tiny_measurements, 1)
        << "the dirty nested subtree should be remeasured exactly once after mutation";
    EXPECT_EQ(delta_measurements, 2);
    EXPECT_GT(engine.relayout_min_content_width_memo_hit_count(), 0)
        << "unchanged nested siblings should hit the relayout min-content memo path";
}

TEST(LayoutTableRegressionTest, TableRelayoutMaxContentIgnoresFloatedMutationV2095) {
    auto table = make_table();
    table->specified_width = 520.0f;

    auto row = make_row();

    auto label_cell = make_cell();
    label_cell->specified_width = 36.0f;
    label_cell->append_child(make_text("1.", 14.0f));

    auto title_cell = make_cell();
    title_cell->white_space = 1;  // nowrap => max-content hint path

    auto stable_inline_block = std::make_unique<LayoutNode>();
    stable_inline_block->tag_name = "span";
    stable_inline_block->mode = LayoutMode::InlineBlock;
    stable_inline_block->display = DisplayType::InlineBlock;
    LayoutNode* stable_inline_block_ptr = stable_inline_block.get();
    stable_inline_block->append_child(make_text("steady cluster", 16.0f));

    auto floated_inline_block = std::make_unique<LayoutNode>();
    floated_inline_block->tag_name = "span";
    floated_inline_block->mode = LayoutMode::InlineBlock;
    floated_inline_block->display = DisplayType::InlineBlock;
    floated_inline_block->float_type = 1;
    auto floated_text = make_text("stretching delta", 16.0f);
    LayoutNode* floated_text_ptr = floated_text.get();
    floated_inline_block->append_child(std::move(floated_text));

    title_cell->append_child(std::move(stable_inline_block));
    title_cell->append_child(std::move(floated_inline_block));
    row->append_child(std::move(label_cell));
    row->append_child(std::move(title_cell));
    table->append_child(std::move(row));

    LayoutEngine engine;
    engine.set_text_measurer([](const std::string& measured, float font_size, const std::string&,
                                int, bool, float) {
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 520.0f, 240.0f);
    const int hits_after_first_compute = engine.relayout_max_content_width_memo_hit_count();
    const float stable_width_before = stable_inline_block_ptr->geometry.width;
    const float title_width_before = table->children[0]->children[1]->geometry.width;

    floated_text_ptr->text_content = "tiny";
    engine.compute(*table, 520.0f, 240.0f);
    const int hits_after_floated_mutation = engine.relayout_max_content_width_memo_hit_count();
    const float stable_width_after = stable_inline_block_ptr->geometry.width;
    const float title_width_after = table->children[0]->children[1]->geometry.width;

    EXPECT_NEAR(stable_width_after, stable_width_before, 0.5f);
    EXPECT_NEAR(title_width_after, title_width_before, 0.5f);
    EXPECT_GT(hits_after_floated_mutation, hits_after_first_compute)
        << "mutating floated descendants should stay outside intrinsic sizing and keep memo hits hot";
}

TEST(LayoutTableRegressionTest, TableIntrinsicRelayoutOutOfFlowParticipationFlipKeepsStableMemoHotV2106) {
    auto table = make_table();
    table->specified_width = 520.0f;

    auto row = make_row();

    auto label_cell = make_cell();
    label_cell->specified_width = 36.0f;
    label_cell->append_child(make_text("1.", 14.0f));

    auto title_cell = make_cell();
    title_cell->white_space = 1;  // nowrap => max-content hint path

    auto stable_inline_block = std::make_unique<LayoutNode>();
    stable_inline_block->tag_name = "span";
    stable_inline_block->mode = LayoutMode::InlineBlock;
    stable_inline_block->display = DisplayType::InlineBlock;
    stable_inline_block->append_child(make_text("steady cluster", 16.0f));

    auto toggled_inline_block = std::make_unique<LayoutNode>();
    toggled_inline_block->tag_name = "span";
    toggled_inline_block->mode = LayoutMode::InlineBlock;
    toggled_inline_block->display = DisplayType::InlineBlock;
    toggled_inline_block->position_type = 2;  // absolute => out of intrinsic flow
    LayoutNode* toggled_inline_block_ptr = toggled_inline_block.get();
    auto toggled_text = make_text("stretching delta", 16.0f);
    LayoutNode* toggled_text_ptr = toggled_text.get();
    toggled_inline_block->append_child(std::move(toggled_text));

    title_cell->append_child(std::move(stable_inline_block));
    title_cell->append_child(std::move(toggled_inline_block));
    row->append_child(std::move(label_cell));
    row->append_child(std::move(title_cell));
    table->append_child(std::move(row));

    LayoutEngine engine;
    engine.set_text_measurer([](const std::string& measured, float font_size, const std::string&,
                                int, bool, float) {
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 520.0f, 240.0f);
    const int hits_after_first_compute = engine.relayout_max_content_width_memo_hit_count();

    toggled_text_ptr->text_content = "tiny";
    engine.compute(*table, 520.0f, 240.0f);
    const int hits_after_out_of_flow_mutation = engine.relayout_max_content_width_memo_hit_count();

    toggled_inline_block_ptr->position_type = 0;
    engine.compute(*table, 520.0f, 240.0f);
    const int hits_after_participation_flip = engine.relayout_max_content_width_memo_hit_count();

    EXPECT_GT(hits_after_out_of_flow_mutation, hits_after_first_compute)
        << "text mutations on an out-of-flow descendant should keep the cached max-content hint hot";
    EXPECT_GT(hits_after_participation_flip, hits_after_out_of_flow_mutation)
        << "after an out-of-flow descendant starts participating, unchanged siblings should still reuse their relayout hints";

    engine.compute(*table, 520.0f, 240.0f);
    EXPECT_GT(engine.relayout_max_content_width_memo_hit_count(), hits_after_participation_flip)
        << "after the participation edge stabilizes, repeated relayout should memo-hit again";
}

TEST(LayoutTableRegressionTest, TableRelayoutColspanMutationKeepsUnrelatedSiblingMemoHotV2096) {
    auto table = make_table();
    table->specified_width = 520.0f;

    auto first_row = make_row();

    auto label_cell = make_cell();
    label_cell->specified_width = 36.0f;
    label_cell->append_child(make_text("1.", 14.0f));

    auto stable_cell = make_cell();
    stable_cell->append_child(make_text("steady badge", 16.0f));

    auto spanning_cell = make_cell();
    spanning_cell->colspan = 2;
    auto mutating_inline_block = std::make_unique<LayoutNode>();
    mutating_inline_block->tag_name = "span";
    mutating_inline_block->mode = LayoutMode::InlineBlock;
    mutating_inline_block->display = DisplayType::InlineBlock;
    auto mutating_text = make_text("stretching delta", 16.0f);
    LayoutNode* mutating_text_ptr = mutating_text.get();
    mutating_inline_block->append_child(std::move(mutating_text));
    spanning_cell->append_child(std::move(mutating_inline_block));

    first_row->append_child(std::move(label_cell));
    first_row->append_child(std::move(stable_cell));
    first_row->append_child(std::move(spanning_cell));

    auto second_row = make_row();
    auto second_label = make_cell();
    second_label->append_child(make_text("meta", 14.0f));
    auto outside_colspan_cell = make_cell();
    outside_colspan_cell->append_child(make_text("steady footer", 14.0f));
    auto shared_colspan_left = make_cell();
    shared_colspan_left->append_child(make_text("shared alpha", 14.0f));
    auto shared_colspan_right = make_cell();
    shared_colspan_right->append_child(make_text("shared beta", 14.0f));

    second_row->append_child(std::move(second_label));
    second_row->append_child(std::move(outside_colspan_cell));
    second_row->append_child(std::move(shared_colspan_left));
    second_row->append_child(std::move(shared_colspan_right));

    table->append_child(std::move(first_row));
    table->append_child(std::move(second_row));

    int steady_measurements = 0;
    int badge_measurements = 0;
    int stretching_measurements = 0;
    int tiny_measurements = 0;
    int delta_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "steady") {
            steady_measurements++;
        } else if (measured == "badge") {
            badge_measurements++;
        } else if (measured == "stretching") {
            stretching_measurements++;
        } else if (measured == "tiny") {
            tiny_measurements++;
        } else if (measured == "delta") {
            delta_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 520.0f, 240.0f);
    const int steady_after_first_compute = steady_measurements;
    const int badge_after_first_compute = badge_measurements;
    const int stretching_after_first_compute = stretching_measurements;
    const int delta_after_first_compute = delta_measurements;

    mutating_text_ptr->text_content = "tiny delta";
    engine.compute(*table, 520.0f, 240.0f);

    EXPECT_EQ(steady_measurements, steady_after_first_compute);
    EXPECT_EQ(badge_measurements, badge_after_first_compute)
        << "stable siblings outside the mutated colspan path should keep their cached intrinsic width hints";
    EXPECT_EQ(stretching_measurements, stretching_after_first_compute);
    EXPECT_EQ(tiny_measurements, 1);
    EXPECT_EQ(delta_measurements, delta_after_first_compute + 1);
}

TEST(LayoutTableRegressionTest, TableRelayoutMinContentMemoInvalidatesOnInlineBlockDescendantMutationV2091) {
    auto table = make_table();
    table->specified_width = 520.0f;

    auto row = make_row();

    auto label_cell = make_cell();
    label_cell->specified_width = 36.0f;
    label_cell->append_child(make_text("1.", 14.0f));

    auto title_cell = make_cell();
    auto inline_block = std::make_unique<LayoutNode>();
    inline_block->tag_name = "span";
    inline_block->mode = LayoutMode::InlineBlock;
    inline_block->display = DisplayType::InlineBlock;
    auto mutating_text = make_text("stretching delta", 16.0f);
    LayoutNode* mutating_text_ptr = mutating_text.get();
    inline_block->append_child(std::move(mutating_text));
    title_cell->append_child(std::move(inline_block));

    row->append_child(std::move(label_cell));
    row->append_child(std::move(title_cell));
    table->append_child(std::move(row));

    LayoutEngine engine;
    engine.set_text_measurer([](const std::string& measured, float font_size, const std::string&,
                                int, bool, float) {
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });
    engine.compute(*table, 520.0f, 240.0f);
    const int hits_after_first_compute = engine.relayout_min_content_width_memo_hit_count();

    table->specified_width = 420.0f;
    engine.compute(*table, 420.0f, 240.0f);
    const int hits_after_first_relayout = engine.relayout_min_content_width_memo_hit_count();
    EXPECT_GT(hits_after_first_relayout, hits_after_first_compute);

    mutating_text_ptr->text_content = "tiny delta";
    engine.compute(*table, 420.0f, 240.0f);
    EXPECT_EQ(engine.relayout_min_content_width_memo_hit_count(), hits_after_first_relayout)
        << "changing an inline-block descendant should invalidate the cached min-content relayout hint";

    engine.compute(*table, 420.0f, 240.0f);
    EXPECT_GT(engine.relayout_min_content_width_memo_hit_count(), hits_after_first_relayout)
        << "once the mutated subtree stabilizes again, repeated relayout should memo-hit again";
}

TEST(LayoutTableRegressionTest, TableIntrinsicRelayoutMinContentReusesStableNestedBlockHintsAfterSiblingMutationV2106) {
    auto table = make_table();
    table->specified_width = 520.0f;

    auto row = make_row();

    auto label_cell = make_cell();
    label_cell->specified_width = 36.0f;
    label_cell->append_child(make_text("1.", 14.0f));

    auto title_cell = make_cell();

    auto stable_wrapper = std::make_unique<LayoutNode>();
    stable_wrapper->tag_name = "div";
    stable_wrapper->mode = LayoutMode::Block;
    stable_wrapper->display = DisplayType::Block;
    stable_wrapper->append_child(make_text("steady alpha beta", 16.0f));

    auto mutating_inline_block = std::make_unique<LayoutNode>();
    mutating_inline_block->tag_name = "span";
    mutating_inline_block->mode = LayoutMode::InlineBlock;
    mutating_inline_block->display = DisplayType::InlineBlock;
    auto mutating_text = make_text("stretching delta epsilon", 16.0f);
    LayoutNode* mutating_text_ptr = mutating_text.get();
    mutating_inline_block->append_child(std::move(mutating_text));

    title_cell->append_child(std::move(stable_wrapper));
    title_cell->append_child(std::move(mutating_inline_block));
    row->append_child(std::move(label_cell));
    row->append_child(std::move(title_cell));
    table->append_child(std::move(row));

    int stable_word_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "steady" || measured == "alpha" || measured == "beta") {
            stable_word_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 520.0f, 240.0f);
    EXPECT_EQ(stable_word_measurements, 3);

    mutating_text_ptr->text_content = "tiny delta";
    table->specified_width = 420.0f;
    engine.compute(*table, 420.0f, 240.0f);

    EXPECT_EQ(stable_word_measurements, 3)
        << "stable nested block descendants should reuse relayout min-content hints when a sibling subtree invalidates the cell";
    EXPECT_GT(engine.relayout_min_content_width_memo_hit_count(), 0)
        << "the unchanged nested block should hit the relayout min-content memo path after the sibling mutation";
}

TEST(LayoutTableRegressionTest, TableRelayoutRowspanMutationKeepsOutsideRowsMemoHotV2097) {
    auto table = make_table();
    table->specified_width = 360.0f;

    auto first_row = make_row();
    auto rowspan_cell = make_cell();
    rowspan_cell->rowspan = 2;
    auto mutating_inline_block = std::make_unique<LayoutNode>();
    mutating_inline_block->tag_name = "span";
    mutating_inline_block->mode = LayoutMode::InlineBlock;
    mutating_inline_block->display = DisplayType::InlineBlock;
    auto mutating_text = make_text("stretching delta", 16.0f);
    LayoutNode* mutating_text_ptr = mutating_text.get();
    mutating_inline_block->append_child(std::move(mutating_text));
    rowspan_cell->append_child(std::move(mutating_inline_block));

    auto first_row_peer = make_cell();
    first_row_peer->append_child(make_text("shared notes", 16.0f));

    first_row->append_child(std::move(rowspan_cell));
    first_row->append_child(std::move(first_row_peer));

    auto second_row = make_row();
    auto second_row_peer = make_cell();
    second_row_peer->append_child(make_text("active lane", 16.0f));
    second_row->append_child(std::move(second_row_peer));

    auto third_row = make_row();
    auto outside_left = make_cell();
    outside_left->append_child(make_text("steady signal", 16.0f));
    auto outside_right = make_cell();
    outside_right->append_child(make_text("calm marker", 16.0f));
    third_row->append_child(std::move(outside_left));
    third_row->append_child(std::move(outside_right));

    table->append_child(std::move(first_row));
    table->append_child(std::move(second_row));
    table->append_child(std::move(third_row));

    int steady_measurements = 0;
    int signal_measurements = 0;
    int calm_measurements = 0;
    int marker_measurements = 0;
    int stretching_measurements = 0;
    int tiny_measurements = 0;
    int delta_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "steady") {
            steady_measurements++;
        } else if (measured == "signal") {
            signal_measurements++;
        } else if (measured == "calm") {
            calm_measurements++;
        } else if (measured == "marker") {
            marker_measurements++;
        } else if (measured == "stretching") {
            stretching_measurements++;
        } else if (measured == "tiny") {
            tiny_measurements++;
        } else if (measured == "delta") {
            delta_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 360.0f, 240.0f);
    const float wide_rowspan_width = table->children[0]->children[0]->geometry.width;

    mutating_text_ptr->text_content = "tiny delta";
    engine.compute(*table, 360.0f, 240.0f);
    const float narrow_rowspan_width = table->children[0]->children[0]->geometry.width;

    EXPECT_LT(narrow_rowspan_width, wide_rowspan_width);
    EXPECT_EQ(steady_measurements, 1);
    EXPECT_EQ(signal_measurements, 1);
    EXPECT_EQ(calm_measurements, 1);
    EXPECT_EQ(marker_measurements, 1)
        << "rows outside the mutated rowspan path should keep their min-content memoized measurements";
    EXPECT_EQ(stretching_measurements, 1);
    EXPECT_EQ(tiny_measurements, 1);
    EXPECT_EQ(delta_measurements, 2)
        << "the mutated rowspan cell should be remeasured exactly once after its intrinsic width changes";
}

TEST(LayoutTableRegressionTest, IntrinsicWidthHintWidthAffectingNowrapMutationKeepsSiblingHintsV2102) {
    auto table = make_table();
    table->specified_width = 320.0f;

    auto row = make_row();
    auto mutating_cell = make_cell();
    LayoutNode* mutating_cell_ptr = mutating_cell.get();
    mutating_cell->append_child(make_text("alpha beta gamma", 16.0f));

    auto steady_cell = make_cell();
    steady_cell->append_child(make_text("steady signal", 16.0f));

    row->append_child(std::move(mutating_cell));
    row->append_child(std::move(steady_cell));
    table->append_child(std::move(row));

    int full_text_measurements = 0;
    int steady_measurements = 0;
    int signal_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "alpha beta gamma") {
            full_text_measurements++;
        } else if (measured == "steady") {
            steady_measurements++;
        } else if (measured == "signal") {
            signal_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 320.0f, 240.0f);
    const float wrapping_first_col = table->children[0]->children[0]->geometry.width;
    const float wrapping_second_col = table->children[0]->children[1]->geometry.width;
    const int steady_after_wrapping = steady_measurements;
    const int signal_after_wrapping = signal_measurements;
    const int full_text_after_wrapping = full_text_measurements;

    mutating_cell_ptr->white_space_nowrap = true;
    engine.compute(*table, 320.0f, 240.0f);
    const float nowrap_first_col = table->children[0]->children[0]->geometry.width;
    const float nowrap_second_col = table->children[0]->children[1]->geometry.width;

    EXPECT_GT(nowrap_first_col, wrapping_first_col);
    EXPECT_LT(nowrap_second_col, wrapping_second_col);
    EXPECT_GT(full_text_measurements, full_text_after_wrapping)
        << "the mutated nowrap cell should recompute its intrinsic width hint";
    EXPECT_EQ(steady_measurements, steady_after_wrapping);
    EXPECT_EQ(signal_measurements, signal_after_wrapping)
        << "stable sibling cells should keep their retained intrinsic width hints";
}

TEST(LayoutTableRegressionTest, IntrinsicWidthHintWidthAffectingExplicitWidthMutationKeepsSiblingHintsV2102) {
    auto table = make_table();
    table->specified_width = 360.0f;

    auto row = make_row();
    auto mutating_cell = make_cell();
    auto explicit_width_box = std::make_unique<LayoutNode>();
    explicit_width_box->tag_name = "span";
    explicit_width_box->mode = LayoutMode::InlineBlock;
    explicit_width_box->display = DisplayType::InlineBlock;
    LayoutNode* explicit_width_box_ptr = explicit_width_box.get();
    explicit_width_box->append_child(make_text("alpha beta", 16.0f));
    mutating_cell->append_child(std::move(explicit_width_box));

    auto steady_cell = make_cell();
    steady_cell->append_child(make_text("steady signal", 16.0f));

    row->append_child(std::move(mutating_cell));
    row->append_child(std::move(steady_cell));
    table->append_child(std::move(row));

    int steady_measurements = 0;
    int signal_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "steady") {
            steady_measurements++;
        } else if (measured == "signal") {
            signal_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 360.0f, 240.0f);
    const float initial_first_col = table->children[0]->children[0]->geometry.width;
    const float initial_second_col = table->children[0]->children[1]->geometry.width;
    const int steady_before_width_mutation = steady_measurements;
    const int signal_before_width_mutation = signal_measurements;

    explicit_width_box_ptr->specified_width = initial_first_col + 80.0f;
    engine.compute(*table, 360.0f, 240.0f);
    const float widened_first_col = table->children[0]->children[0]->geometry.width;
    const float narrowed_second_col = table->children[0]->children[1]->geometry.width;

    EXPECT_GT(widened_first_col, initial_first_col + 20.0f);
    EXPECT_LT(narrowed_second_col, initial_second_col);
    EXPECT_EQ(steady_measurements, steady_before_width_mutation);
    EXPECT_EQ(signal_measurements, signal_before_width_mutation)
        << "explicit width mutations should preserve cached sibling hints outside the dirty path";
}

TEST(LayoutTableRegressionTest,
     IntrinsicWidthHintWidthAffectingCellExplicitWidthFlipFlopKeepsSiblingHintsV2104) {
    auto table = make_table();
    table->specified_width = 360.0f;

    auto row = make_row();
    auto mutating_cell = make_cell();
    LayoutNode* mutating_cell_ptr = mutating_cell.get();
    auto mutating_text = make_text("stretching delta", 16.0f);
    LayoutNode* mutating_text_ptr = mutating_text.get();
    mutating_cell->append_child(std::move(mutating_text));

    auto steady_cell = make_cell();
    steady_cell->append_child(make_text("steady signal", 16.0f));

    row->append_child(std::move(mutating_cell));
    row->append_child(std::move(steady_cell));
    table->append_child(std::move(row));

    int steady_measurements = 0;
    int signal_measurements = 0;
    int stretching_measurements = 0;
    int tiny_measurements = 0;
    int delta_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "steady") {
            steady_measurements++;
        } else if (measured == "signal") {
            signal_measurements++;
        } else if (measured == "stretching") {
            stretching_measurements++;
        } else if (measured == "tiny") {
            tiny_measurements++;
        } else if (measured == "delta") {
            delta_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 360.0f, 240.0f);
    const float initial_first_col = table->children[0]->children[0]->geometry.width;
    const int steady_before_width_toggle = steady_measurements;
    const int signal_before_width_toggle = signal_measurements;

    mutating_cell_ptr->specified_width = initial_first_col + 80.0f;
    mutating_text_ptr->text_content = "tiny delta";
    engine.compute(*table, 360.0f, 240.0f);
    const float explicit_first_col = table->children[0]->children[0]->geometry.width;

    mutating_cell_ptr->specified_width = -1.0f;
    engine.compute(*table, 360.0f, 240.0f);
    const float restored_auto_first_col = table->children[0]->children[0]->geometry.width;

    EXPECT_GT(explicit_first_col, initial_first_col + 20.0f);
    EXPECT_LT(restored_auto_first_col, explicit_first_col)
        << "returning from an explicit width edit should drop the stale explicit-width path";
    EXPECT_EQ(steady_measurements, steady_before_width_toggle);
    EXPECT_EQ(signal_measurements, signal_before_width_toggle)
        << "cell explicit-width flip-flops should preserve cached sibling hints";
    EXPECT_EQ(stretching_measurements, 1);
    EXPECT_EQ(tiny_measurements, 1);
    EXPECT_EQ(delta_measurements, 2)
        << "restoring auto width should force one fresh intrinsic measurement for the mutated cell";
}

TEST(LayoutTableRegressionTest, IntrinsicWidthHintWidthAffectingNestedSubtreeMutationKeepsSiblingHintsV2102) {
    auto table = make_table();
    table->specified_width = 320.0f;

    auto row = make_row();
    auto cell = make_cell();

    auto stable_inline_block = std::make_unique<LayoutNode>();
    stable_inline_block->tag_name = "span";
    stable_inline_block->mode = LayoutMode::InlineBlock;
    stable_inline_block->display = DisplayType::InlineBlock;
    stable_inline_block->append_child(make_text("steady signal", 16.0f));

    auto mutating_inline_block = std::make_unique<LayoutNode>();
    mutating_inline_block->tag_name = "span";
    mutating_inline_block->mode = LayoutMode::InlineBlock;
    mutating_inline_block->display = DisplayType::InlineBlock;
    LayoutNode* mutating_inline_block_ptr = mutating_inline_block.get();
    auto mutating_text = make_text("stretching delta", 16.0f);
    LayoutNode* mutating_text_ptr = mutating_text.get();
    mutating_inline_block->append_child(std::move(mutating_text));

    cell->append_child(std::move(stable_inline_block));
    cell->append_child(std::move(mutating_inline_block));
    row->append_child(std::move(cell));
    table->append_child(std::move(row));

    int steady_measurements = 0;
    int signal_measurements = 0;
    int stretching_measurements = 0;
    int tiny_measurements = 0;
    int delta_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "steady") {
            steady_measurements++;
        } else if (measured == "signal") {
            signal_measurements++;
        } else if (measured == "stretching") {
            stretching_measurements++;
        } else if (measured == "tiny") {
            tiny_measurements++;
        } else if (measured == "delta") {
            delta_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 320.0f, 240.0f);
    const float wide_mutating_width = mutating_inline_block_ptr->geometry.width;
    const int steady_before_mutation = steady_measurements;
    const int signal_before_mutation = signal_measurements;

    mutating_text_ptr->text_content = "tiny delta";
    engine.compute(*table, 320.0f, 240.0f);
    const float narrow_mutating_width = mutating_inline_block_ptr->geometry.width;

    EXPECT_LT(narrow_mutating_width, wide_mutating_width);
    EXPECT_EQ(steady_measurements, steady_before_mutation);
    EXPECT_EQ(signal_measurements, signal_before_mutation)
        << "nested subtree mutations should preserve sibling memo hits inside the same cell";
    EXPECT_EQ(stretching_measurements, 1);
    EXPECT_EQ(tiny_measurements, 1);
    EXPECT_EQ(delta_measurements, 2);
}

TEST(LayoutTableRegressionTest, IntrinsicWidthHintWidthAffectingColspanMutationKeepsOutsideHintsV2102) {
    auto table = make_table();
    table->specified_width = 520.0f;

    auto first_row = make_row();

    auto label_cell = make_cell();
    label_cell->specified_width = 36.0f;
    label_cell->append_child(make_text("1.", 14.0f));

    auto steady_cell = make_cell();
    steady_cell->append_child(make_text("steady badge", 16.0f));

    auto spanning_cell = make_cell();
    spanning_cell->colspan = 2;
    auto mutating_inline_block = std::make_unique<LayoutNode>();
    mutating_inline_block->tag_name = "span";
    mutating_inline_block->mode = LayoutMode::InlineBlock;
    mutating_inline_block->display = DisplayType::InlineBlock;
    auto mutating_text = make_text("stretching delta", 16.0f);
    LayoutNode* mutating_text_ptr = mutating_text.get();
    mutating_inline_block->append_child(std::move(mutating_text));
    spanning_cell->append_child(std::move(mutating_inline_block));

    first_row->append_child(std::move(label_cell));
    first_row->append_child(std::move(steady_cell));
    first_row->append_child(std::move(spanning_cell));

    auto second_row = make_row();
    auto second_label = make_cell();
    second_label->append_child(make_text("meta", 14.0f));
    auto outside_colspan_cell = make_cell();
    outside_colspan_cell->append_child(make_text("steady footer", 14.0f));
    auto shared_colspan_left = make_cell();
    shared_colspan_left->append_child(make_text("shared alpha", 14.0f));
    auto shared_colspan_right = make_cell();
    shared_colspan_right->append_child(make_text("shared beta", 14.0f));

    second_row->append_child(std::move(second_label));
    second_row->append_child(std::move(outside_colspan_cell));
    second_row->append_child(std::move(shared_colspan_left));
    second_row->append_child(std::move(shared_colspan_right));

    table->append_child(std::move(first_row));
    table->append_child(std::move(second_row));

    int steady_measurements = 0;
    int badge_measurements = 0;
    int stretching_measurements = 0;
    int tiny_measurements = 0;
    int delta_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "steady") {
            steady_measurements++;
        } else if (measured == "badge") {
            badge_measurements++;
        } else if (measured == "stretching") {
            stretching_measurements++;
        } else if (measured == "tiny") {
            tiny_measurements++;
        } else if (measured == "delta") {
            delta_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 520.0f, 240.0f);
    const int steady_before_mutation = steady_measurements;
    const int badge_before_mutation = badge_measurements;

    mutating_text_ptr->text_content = "tiny delta";
    engine.compute(*table, 520.0f, 240.0f);

    EXPECT_EQ(steady_measurements, steady_before_mutation);
    EXPECT_EQ(badge_measurements, badge_before_mutation)
        << "colspan-path invalidation should preserve cached hints for unaffected siblings";
    EXPECT_EQ(stretching_measurements, 1);
    EXPECT_EQ(tiny_measurements, 1);
    EXPECT_EQ(delta_measurements, 2);
}

TEST(LayoutTableRegressionTest, IntrinsicWidthHintWidthAffectingRowspanMutationKeepsOutsideHintsV2102) {
    auto table = make_table();
    table->specified_width = 360.0f;

    auto first_row = make_row();
    auto rowspan_cell = make_cell();
    rowspan_cell->rowspan = 2;
    auto mutating_inline_block = std::make_unique<LayoutNode>();
    mutating_inline_block->tag_name = "span";
    mutating_inline_block->mode = LayoutMode::InlineBlock;
    mutating_inline_block->display = DisplayType::InlineBlock;
    auto mutating_text = make_text("stretching delta", 16.0f);
    LayoutNode* mutating_text_ptr = mutating_text.get();
    mutating_inline_block->append_child(std::move(mutating_text));
    rowspan_cell->append_child(std::move(mutating_inline_block));

    auto first_row_peer = make_cell();
    first_row_peer->append_child(make_text("shared notes", 16.0f));

    first_row->append_child(std::move(rowspan_cell));
    first_row->append_child(std::move(first_row_peer));

    auto second_row = make_row();
    auto second_row_peer = make_cell();
    second_row_peer->append_child(make_text("active lane", 16.0f));
    second_row->append_child(std::move(second_row_peer));

    auto third_row = make_row();
    auto outside_left = make_cell();
    outside_left->append_child(make_text("steady signal", 16.0f));
    auto outside_right = make_cell();
    outside_right->append_child(make_text("calm marker", 16.0f));
    third_row->append_child(std::move(outside_left));
    third_row->append_child(std::move(outside_right));

    table->append_child(std::move(first_row));
    table->append_child(std::move(second_row));
    table->append_child(std::move(third_row));

    int steady_measurements = 0;
    int signal_measurements = 0;
    int calm_measurements = 0;
    int marker_measurements = 0;
    int stretching_measurements = 0;
    int tiny_measurements = 0;
    int delta_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "steady") {
            steady_measurements++;
        } else if (measured == "signal") {
            signal_measurements++;
        } else if (measured == "calm") {
            calm_measurements++;
        } else if (measured == "marker") {
            marker_measurements++;
        } else if (measured == "stretching") {
            stretching_measurements++;
        } else if (measured == "tiny") {
            tiny_measurements++;
        } else if (measured == "delta") {
            delta_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 360.0f, 240.0f);
    const float wide_rowspan_width = table->children[0]->children[0]->geometry.width;
    const int steady_before_mutation = steady_measurements;
    const int signal_before_mutation = signal_measurements;
    const int calm_before_mutation = calm_measurements;
    const int marker_before_mutation = marker_measurements;

    mutating_text_ptr->text_content = "tiny delta";
    engine.compute(*table, 360.0f, 240.0f);
    const float narrow_rowspan_width = table->children[0]->children[0]->geometry.width;

    EXPECT_LT(narrow_rowspan_width, wide_rowspan_width);
    EXPECT_EQ(steady_measurements, steady_before_mutation);
    EXPECT_EQ(signal_measurements, signal_before_mutation);
    EXPECT_EQ(calm_measurements, calm_before_mutation);
    EXPECT_EQ(marker_measurements, marker_before_mutation)
        << "rowspan-path invalidation should preserve cached hints for unaffected rows";
    EXPECT_EQ(stretching_measurements, 1);
    EXPECT_EQ(tiny_measurements, 1);
    EXPECT_EQ(delta_measurements, 2);
}

TEST(LayoutTableRegressionTest, IntrinsicWidthHintColspanTextMutationKeepsSharedSiblingHintsHotV2107) {
    auto table = make_table();
    table->specified_width = 520.0f;

    auto first_row = make_row();

    auto label_cell = make_cell();
    label_cell->specified_width = 36.0f;
    label_cell->append_child(make_text("1.", 14.0f));

    auto steady_cell = make_cell();
    steady_cell->append_child(make_text("steady badge", 16.0f));

    auto spanning_cell = make_cell();
    spanning_cell->colspan = 2;
    auto mutating_inline_block = std::make_unique<LayoutNode>();
    mutating_inline_block->tag_name = "span";
    mutating_inline_block->mode = LayoutMode::InlineBlock;
    mutating_inline_block->display = DisplayType::InlineBlock;
    auto mutating_text = make_text("stretching delta", 16.0f);
    LayoutNode* mutating_text_ptr = mutating_text.get();
    mutating_inline_block->append_child(std::move(mutating_text));
    spanning_cell->append_child(std::move(mutating_inline_block));

    first_row->append_child(std::move(label_cell));
    first_row->append_child(std::move(steady_cell));
    first_row->append_child(std::move(spanning_cell));

    auto second_row = make_row();
    auto second_label = make_cell();
    second_label->append_child(make_text("meta", 14.0f));
    auto outside_colspan_cell = make_cell();
    outside_colspan_cell->append_child(make_text("steady footer", 14.0f));
    auto shared_colspan_left = make_cell();
    shared_colspan_left->append_child(make_text("shared alpha", 14.0f));
    auto shared_colspan_right = make_cell();
    shared_colspan_right->append_child(make_text("shared beta", 14.0f));

    second_row->append_child(std::move(second_label));
    second_row->append_child(std::move(outside_colspan_cell));
    second_row->append_child(std::move(shared_colspan_left));
    second_row->append_child(std::move(shared_colspan_right));

    table->append_child(std::move(first_row));
    table->append_child(std::move(second_row));

    int alpha_measurements = 0;
    int beta_measurements = 0;
    int stretching_measurements = 0;
    int tiny_measurements = 0;
    int delta_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "alpha") {
            alpha_measurements++;
        } else if (measured == "beta") {
            beta_measurements++;
        } else if (measured == "stretching") {
            stretching_measurements++;
        } else if (measured == "tiny") {
            tiny_measurements++;
        } else if (measured == "delta") {
            delta_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 520.0f, 240.0f);
    const int alpha_before_mutation = alpha_measurements;
    const int beta_before_mutation = beta_measurements;

    mutating_text_ptr->text_content = "tiny delta";
    engine.compute(*table, 520.0f, 240.0f);

    EXPECT_EQ(alpha_measurements, alpha_before_mutation);
    EXPECT_EQ(beta_measurements, beta_before_mutation)
        << "shared cells under the same colspan path should keep their cached intrinsic hints";
    EXPECT_EQ(stretching_measurements, 1);
    EXPECT_EQ(tiny_measurements, 1);
    EXPECT_EQ(delta_measurements, 2);
}

TEST(LayoutTableRegressionTest, IntrinsicWidthHintRowspanTextMutationKeepsSharedSiblingHintsHotV2107) {
    auto table = make_table();
    table->specified_width = 360.0f;

    auto first_row = make_row();
    auto rowspan_cell = make_cell();
    rowspan_cell->rowspan = 2;
    auto mutating_inline_block = std::make_unique<LayoutNode>();
    mutating_inline_block->tag_name = "span";
    mutating_inline_block->mode = LayoutMode::InlineBlock;
    mutating_inline_block->display = DisplayType::InlineBlock;
    auto mutating_text = make_text("stretching delta", 16.0f);
    LayoutNode* mutating_text_ptr = mutating_text.get();
    mutating_inline_block->append_child(std::move(mutating_text));
    rowspan_cell->append_child(std::move(mutating_inline_block));

    auto first_row_peer = make_cell();
    first_row_peer->append_child(make_text("shared notes", 16.0f));

    first_row->append_child(std::move(rowspan_cell));
    first_row->append_child(std::move(first_row_peer));

    auto second_row = make_row();
    auto second_row_peer = make_cell();
    second_row_peer->append_child(make_text("active lane", 16.0f));
    second_row->append_child(std::move(second_row_peer));

    auto third_row = make_row();
    auto outside_left = make_cell();
    outside_left->append_child(make_text("steady signal", 16.0f));
    auto outside_right = make_cell();
    outside_right->append_child(make_text("calm marker", 16.0f));
    third_row->append_child(std::move(outside_left));
    third_row->append_child(std::move(outside_right));

    table->append_child(std::move(first_row));
    table->append_child(std::move(second_row));
    table->append_child(std::move(third_row));

    int notes_measurements = 0;
    int active_measurements = 0;
    int lane_measurements = 0;
    int stretching_measurements = 0;
    int tiny_measurements = 0;
    int delta_measurements = 0;

    LayoutEngine engine;
    engine.set_text_measurer([&](const std::string& measured, float font_size, const std::string&,
                                 int, bool, float) {
        if (measured == "notes") {
            notes_measurements++;
        } else if (measured == "active") {
            active_measurements++;
        } else if (measured == "lane") {
            lane_measurements++;
        } else if (measured == "stretching") {
            stretching_measurements++;
        } else if (measured == "tiny") {
            tiny_measurements++;
        } else if (measured == "delta") {
            delta_measurements++;
        }
        return static_cast<float>(measured.size()) * font_size * 0.6f;
    });

    engine.compute(*table, 360.0f, 240.0f);
    const int notes_before_mutation = notes_measurements;
    const int active_before_mutation = active_measurements;
    const int lane_before_mutation = lane_measurements;

    mutating_text_ptr->text_content = "tiny delta";
    engine.compute(*table, 360.0f, 240.0f);

    EXPECT_EQ(notes_measurements, notes_before_mutation);
    EXPECT_EQ(active_measurements, active_before_mutation);
    EXPECT_EQ(lane_measurements, lane_before_mutation)
        << "shared rows under the same rowspan path should keep their cached intrinsic hints";
    EXPECT_EQ(stretching_measurements, 1);
    EXPECT_EQ(tiny_measurements, 1);
    EXPECT_EQ(delta_measurements, 2);
}

TEST(TableLayoutRegression, AutoLayoutKeepsColspanAndRowspanPlacementStableAfterSecondPass) {
    auto table = make_rowspan_colspan_table(360.0f);

    LayoutEngine engine;
    engine.compute(*table, 360.0f, 240.0f);

    table->specified_width = 300.0f;
    engine.compute(*table, 300.0f, 240.0f);

    ASSERT_EQ(table->children.size(), 2u);
    auto* laid_first_row = table->children[0].get();
    auto* laid_second_row = table->children[1].get();
    ASSERT_EQ(laid_first_row->children.size(), 2u);
    ASSERT_EQ(laid_second_row->children.size(), 2u);

    auto* laid_rowspan_cell = laid_first_row->children[0].get();
    auto* laid_wide_cell = laid_first_row->children[1].get();
    auto* laid_meta_cell = laid_second_row->children[0].get();
    auto* laid_comments_cell = laid_second_row->children[1].get();

    EXPECT_GT(laid_rowspan_cell->geometry.width, 40.0f);
    EXPECT_GT(laid_wide_cell->geometry.width, laid_comments_cell->geometry.width);
    EXPECT_NEAR(laid_wide_cell->geometry.width,
                laid_meta_cell->geometry.width + laid_comments_cell->geometry.width,
                0.1f);
    EXPECT_NEAR(laid_meta_cell->geometry.x, laid_rowspan_cell->geometry.width, 0.1f);
    EXPECT_NEAR(laid_comments_cell->geometry.x,
                laid_meta_cell->geometry.x + laid_meta_cell->geometry.width,
                0.1f);
    EXPECT_GE(laid_rowspan_cell->geometry.height,
              laid_first_row->geometry.height + laid_second_row->geometry.height);
}
