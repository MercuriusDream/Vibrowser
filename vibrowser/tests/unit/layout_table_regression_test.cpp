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
