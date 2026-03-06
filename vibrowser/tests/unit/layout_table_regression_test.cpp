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
    auto table = make_table();
    table->specified_width = 360.0f;

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
