// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents"s);
    }
}

void TestAddDocument() {
    SearchServer server;
    server.SetStopWords("with"s);
    server.AddDocument(1, "cat with name"s, DocumentStatus::ACTUAL, { 5, 5 , 5 });
    const auto found_docs = server.FindTopDocuments("cat"s);
    ASSERT_EQUAL(found_docs.size(), 1u);
}

void TestExcludeMinusWords() {
    SearchServer server;
    server.SetStopWords("with"s);
    server.AddDocument(1, "cat with name"s, DocumentStatus::ACTUAL, { 5, 5 , 5 });
    server.AddDocument(2, "cat with black tail"s, DocumentStatus::ACTUAL, { 5, 5 , 5 });
    const auto found_docs = server.FindTopDocuments("cat -black"s);
    ASSERT_EQUAL(found_docs.size(), 1u);
}

void TestMatchWords() {
    SearchServer server;
    server.SetStopWords("with"s);
    server.AddDocument(1, "cat with name"s, DocumentStatus::ACTUAL, { 5, 5 , 5 });
    server.AddDocument(2, "cat with black tail"s, DocumentStatus::ACTUAL, { 5, 5 , 5 });
    const auto matched_words = server.MatchDocument("black cat"s, 1);
    vector<string> a = get<0>(matched_words);
    vector<string> needed_vec = { "cat"s };
    ASSERT_EQUAL(a, needed_vec);

    const auto matched_words_2 = server.MatchDocument("black -cat"s, 2);
    vector<string> b = get<0>(matched_words_2);
    ASSERT(b.empty());
}

void TestSortByRelevance() {
    SearchServer server;
    server.SetStopWords("with"s);
    server.AddDocument(1, "cat with name"s, DocumentStatus::ACTUAL, { 5, 5 , 5 });
    server.AddDocument(2, "cat with black tail"s, DocumentStatus::ACTUAL, { 5, 5 , 5 });
    server.AddDocument(3, "beautiful cat with black tail"s, DocumentStatus::ACTUAL, { 5, 5 , 5 });
    const auto found_docs = server.FindTopDocuments("beautiful cat with tail"s);
    vector<int> a;
    for (const auto& doc : found_docs) {
        a.push_back(doc.id);
    }
    vector<int> b = { 3, 2, 1 };
    ASSERT_EQUAL(a, b);
}

void TestCalcRating() {
    SearchServer server;
    server.SetStopWords("with"s);
    server.AddDocument(1, "cat with name"s, DocumentStatus::ACTUAL, { 5, 5 , 5 });
    server.AddDocument(2, "cat with name"s, DocumentStatus::ACTUAL, { -5, -5 , -5 });
    const auto found_docs = server.FindTopDocuments("cat"s);
    ASSERT_EQUAL(found_docs[0].rating, 5);
    ASSERT_EQUAL(found_docs[1].rating, -5);
}

void TestFindDocumentsWithPredicate() {
    SearchServer server;
    server.SetStopWords("with"s);
    server.AddDocument(1, "cat with name"s, DocumentStatus::ACTUAL, { 5, 5 , 5 });
    server.AddDocument(2, "cat with black tail"s, DocumentStatus::ACTUAL, { 5, 5 , 5 });
    server.AddDocument(3, "beautiful cat with black tail"s, DocumentStatus::ACTUAL, { 5, 5 , 5 });
    const auto found_docs = server.FindTopDocuments("cat", [](int document_id, DocumentStatus status,
        int rating) {return document_id % 2 == 0; });
    ASSERT_EQUAL(found_docs[0].id, 2);
}

void TestFindDocumentsWithStatus() {
    SearchServer server;
    server.SetStopWords("with"s);
    server.AddDocument(1, "cat with name"s, DocumentStatus::REMOVED, { 5, 5 , 5 });
    server.AddDocument(2, "cat with black tail"s, DocumentStatus::BANNED, { 5, 5 , 5 });
    server.AddDocument(3, "beautiful cat with black tail"s, DocumentStatus::IRRELEVANT, { 5, 5 , 5 });
    const auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(found_docs[0].id, 2);
    const auto docs = server.FindTopDocuments("cat"s, DocumentStatus::IRRELEVANT);
    ASSERT_EQUAL(docs[0].id, 3);
}

void TestCorrectRelevance() {
    SearchServer server;
    server.SetStopWords("with and"s);
    server.AddDocument(1, "cat with name"s, DocumentStatus::ACTUAL, { 5, 5 , 5 });
    server.AddDocument(2, "cat with black tail and dog"s, DocumentStatus::ACTUAL, { 5, 5 , 5 });
    server.AddDocument(3, "beautiful cat with black tail"s, DocumentStatus::ACTUAL, { 5, 5 , 5 });
    const auto found_docs = server.FindTopDocuments("cat with black dog"s);
    double a = found_docs[0].relevance;
    double b = 0.376019;
    const double ERROR_VALUE = 1e-6;
    ASSERT(abs(a - b) < ERROR_VALUE);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestExcludeMinusWords);
    RUN_TEST(TestMatchWords);
    RUN_TEST(TestSortByRelevance);
    RUN_TEST(TestCalcRating);
    RUN_TEST(TestFindDocumentsWithPredicate);
    RUN_TEST(TestFindDocumentsWithStatus);
    RUN_TEST(TestCorrectRelevance);
}

// --------- Окончание модульных тестов поисковой системы -----------